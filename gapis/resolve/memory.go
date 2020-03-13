// Copyright (C) 2017 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

package resolve

import (
	"context"
	"fmt"
	"reflect"

	"github.com/google/gapid/core/app/analytics"
	coreid "github.com/google/gapid/core/data/id"
	"github.com/google/gapid/core/math/interval"
	"github.com/google/gapid/gapis/api"
	"github.com/google/gapid/gapis/api/sync"
	"github.com/google/gapid/gapis/capture"
	"github.com/google/gapid/gapis/memory"
	"github.com/google/gapid/gapis/messages"
	"github.com/google/gapid/gapis/service"
	"github.com/google/gapid/gapis/service/memory_box"
	"github.com/google/gapid/gapis/service/path"
	"github.com/google/gapid/gapis/service/types"
)

var (
	tyPointer = reflect.TypeOf((*memory.ReflectPointer)(nil)).Elem()
	tySlice   = reflect.TypeOf((*api.Slice)(nil)).Elem()
)

// Memory resolves and returns the memory from the path p.
func Memory(ctx context.Context, p *path.Memory, rc *path.ResolveConfig) (*service.Memory, error) {
	ctx = SetupContext(ctx, path.FindCapture(p), rc)

	cmdIdx := p.After.Indices[0]
	fullCmdIdx := p.After.Indices

	allCmds, err := Cmds(ctx, path.FindCapture(p))
	if err != nil {
		return nil, err
	}

	if count := uint64(len(allCmds)); cmdIdx >= count {
		return nil, errPathOOB(cmdIdx, "Index", 0, count-1, p)
	}

	sd, err := SyncData(ctx, path.FindCapture(p))
	if err != nil {
		return nil, err
	}

	cmds, err := sync.MutationCmdsFor(ctx, path.FindCapture(p), sd, allCmds, api.CmdID(cmdIdx), fullCmdIdx[1:], true)
	if err != nil {
		return nil, err
	}

	defer analytics.SendTiming("resolve", "memory")(analytics.Count(len(cmds)))

	s, err := capture.NewState(ctx)
	if err != nil {
		return nil, err
	}
	err = api.ForeachCmd(ctx, cmds[:len(cmds)-1], true, func(ctx context.Context, id api.CmdID, cmd api.Cmd) error {
		if err := cmd.Mutate(ctx, id, s, nil, nil); err != nil {
			return fmt.Errorf("Fail to mutate command %v: %v", cmd, err)
		}
		return nil
	})
	if err != nil {
		return nil, err
	}

	r := memory.Range{Base: p.Address, Size: p.Size}
	var reads, writes, observed memory.RangeList
	typedRanges := service.TypedMemoryRanges{}

	s.Memory.SetOnCreate(func(id memory.PoolID, pool *memory.Pool) {
		if id == memory.PoolID(p.Pool) {
			pool.OnRead = func(rng memory.Range, root uint64, id uint64, apiId coreid.ID) {
				if rng.Overlaps(r) {
					interval.Merge(&reads, rng.Window(r).Span(), false)
					if p.IncludeTypes {
						typedRanges = append(typedRanges,
							&service.TypedMemoryRange{
								Type: &path.Type{
									TypeIndex: id,
									API:       &path.API{ID: path.NewID(apiId)},
								},
								Range: &service.MemoryRange{
									Base: rng.Base,
									Size: rng.Size,
								},
								Root: root,
							},
						)
					}
				}
			}
			pool.OnWrite = func(rng memory.Range, root uint64, id uint64, apiId coreid.ID) {
				if rng.Overlaps(r) {
					interval.Merge(&writes, rng.Window(r).Span(), false)
					if p.IncludeTypes {
						typedRanges = append(typedRanges,
							&service.TypedMemoryRange{
								Type: &path.Type{
									TypeIndex: id,
									API:       &path.API{ID: path.NewID(apiId)},
								},
								Range: &service.MemoryRange{
									Base: rng.Base,
									Size: rng.Size,
								},
								Root: root,
							},
						)
					}
				}
			}
		}
	})

	lastCmd := cmds[len(cmds)-1]
	err = api.MutateCmds(ctx, s, nil, nil, lastCmd)
	if err != nil {
		return nil, err
	}

	typedRanges.Filter()

	// Check whether the requested pool was ever created.
	pool, err := s.Memory.Get(memory.PoolID(p.Pool))
	if err != nil {
		return nil, &service.ErrDataUnavailable{Reason: messages.ErrInvalidMemoryPool(p.Pool)}
	}

	slice := pool.Slice(r)

	if !p.ExcludeObserved {
		observed = slice.ValidRanges()
	}

	var data []byte
	if !p.ExcludeData && slice.Size() > 0 {
		data = make([]byte, slice.Size())
		if err := slice.Get(ctx, 0, data); err != nil {
			return nil, err
		}
	}

	return &service.Memory{
		Data:        data,
		Reads:       service.NewMemoryRanges(reads),
		Writes:      service.NewMemoryRanges(writes),
		Observed:    service.NewMemoryRanges(observed),
		TypedRanges: typedRanges,
	}, nil
}

// MemoryAsType resolves and returns the memory from the path p.
func MemoryAsType(ctx context.Context, p *path.MemoryAsType, rc *path.ResolveConfig) (*memory_box.Value, error) {
	ctx = SetupContext(ctx, path.FindCapture(p), rc)

	cmdIdx := p.After.Indices[0]
	fullCmdIdx := p.After.Indices

	allCmds, err := Cmds(ctx, path.FindCapture(p))
	if err != nil {
		return nil, err
	}

	if count := uint64(len(allCmds)); cmdIdx >= count {
		return nil, errPathOOB(cmdIdx, "Index", 0, count-1, p)
	}

	sd, err := SyncData(ctx, path.FindCapture(p))
	if err != nil {
		return nil, err
	}

	cmds, err := sync.MutationCmdsFor(ctx, path.FindCapture(p), sd, allCmds, api.CmdID(cmdIdx), fullCmdIdx[1:], true)
	if err != nil {
		return nil, err
	}

	defer analytics.SendTiming("resolve", "memory")(analytics.Count(len(cmds)))

	s, err := capture.NewState(ctx)
	if err != nil {
		return nil, err
	}
	err = api.ForeachCmd(ctx, cmds, true, func(ctx context.Context, id api.CmdID, cmd api.Cmd) error {
		if err := cmd.Mutate(ctx, id, s, nil, nil); err != nil {
			return fmt.Errorf("Fail to mutate command %v: %v", cmd, err)
		}
		return nil
	})
	if err != nil {
		return nil, err
	}

	// Check whether the requested pool was ever created.
	pool, err := s.Memory.Get(memory.PoolID(p.Pool))
	if err != nil {
		return nil, &service.ErrDataUnavailable{Reason: messages.ErrInvalidMemoryPool(p.Pool)}
	}
	sz := p.Size
	if sz == 0 {
		sz = 0xFFFFFFFFFFFFFFFF
	}

	dec := s.MemoryDecoder(ctx, pool.Slice(memory.Range{
		Base: p.Address,
		Size: sz,
	}))

	e, err := types.GetType(p.Type.TypeIndex)
	if err != nil {
		return nil, err
	}
	return memory_box.DecodeMemory(ctx, s.MemoryLayout, dec, p.Size, e)
}
