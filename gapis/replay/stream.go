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

package replay

import (
	"bytes"
	"context"
	"fmt"
	"reflect"
	"sync/atomic"

	"github.com/google/gapid/core/app/status"
	"github.com/google/gapid/core/data/endian"
	"github.com/google/gapid/core/data/id"
	"github.com/google/gapid/core/log"
	"github.com/google/gapid/core/os/device/bind"
	"github.com/google/gapid/gapis/api"
	"github.com/google/gapid/gapis/capture"
	"github.com/google/gapid/gapis/database"
	"github.com/google/gapid/gapis/memory"
	"github.com/google/gapid/gapis/messages"
	"github.com/google/gapid/gapis/resolve/initialcmds"
	"github.com/google/gapid/gapis/service"
	"github.com/google/gapid/gapis/service/memory_box"
	"github.com/google/gapid/gapis/service/path"
	"github.com/google/gapid/gapis/service/types"
)

var tyPointer = reflect.TypeOf((*memory.ReflectPointer)(nil)).Elem()

type streamConfig struct {
	val uint64
}

var ops uint64

type streamCommunication interface {
	GetUserResponse() (*service.StreamCommandsRequest, error)
	OnCallback(context.Context, *api.Command)
	OnRequestReturn(context.Context, *service.StreamCommandsResponse)
	OnError(context.Context, error)
	OnDone(context.Context)
}

type streamer struct {
	req  *service.StreamStartRequest
	comm streamCommunication
}

func resolveObject(ctx context.Context, command api.Cmd, st *api.GlobalState, base uint64, t uint64, offs uint64) (*memory_box.Value, error) {
	// Check whether the requested pool was ever created.
	pool, err := st.Memory.Get(memory.ApplicationPool)
	if err != nil {
		return nil, &service.ErrDataUnavailable{Reason: messages.ErrInvalidMemoryPool(memory.ApplicationPool)}
	}

	e, err := types.GetType(t)
	if err != nil {
		return nil, err
	}

	sz, err := e.Size(ctx, st.MemoryLayout)
	if err != nil {
		return nil, err
	}

	dec := st.MemoryDecoder(ctx, pool.Slice(memory.Range{
		Base: base + (uint64(sz) * offs),
		Size: uint64(sz),
	}))
	b := make([]byte, sz)
	dec2 := st.MemoryDecoder(ctx, pool.Slice(memory.Range{
		Base: base + (uint64(sz) * offs),
		Size: uint64(sz),
	}))
	dec2.Data(b)

	cm, err := memory_box.DecodeMemory(ctx, st.MemoryLayout, dec, uint64(sz), e)
	return cm, err
}

type mockDevice struct {
	bind.Simple
}

func (*mockDevice) CanTrace() bool {
	return false
}

type write struct {
	rng memory.Range
	id id.ID
}

func Stream(
	ctx context.Context,
	req *service.StreamStartRequest,
	comm streamCommunication) error {
	log.E(ctx, "Start Stream Request %+v\n", req.GetCapture())
	deviceID := id.ID{}
	if req.GetReplayDevice() != nil {
		deviceID = req.GetReplayDevice().ID.ID()
	}
	c, err := capture.ResolveGraphicsFromPath(ctx, req.GetCapture())
	if err != nil {
		log.Err(ctx, err, "Could not resolve capture")
		return err
	}
	if deviceID == (id.ID{}) {
		instance := *c.Header.Device
		instance.Name = fmt.Sprintf("mock-%s-%d", instance.Name, atomic.AddUint64(&ops, 1))
		instance.GenID()
		dev := &mockDevice{}
		dev.To = &instance
		bind.GetRegistry(ctx).AddDevice(ctx, dev)
		deviceID = dev.Instance().ID.ID()
	}

	m := make(map[string]bool)
	for _, x := range req.GetCommandNames() {
		m[x] = true
	}

	ctx = status.Start(ctx, "Stream")
	defer status.Finish(ctx)
	var state *api.GlobalState
	ic := []api.Cmd{}
	if req.IncludeInitialCommands {
		initialCmds, ranges, _ := initialcmds.InitialCommands(ctx, req.GetCapture())
		state = c.NewUninitializedState(ctx).ReserveMemory(ranges)
		ic = initialCmds
	} else {
		state = c.NewState(ctx)
	}
	backup_state := state.Clone(ctx)
	lastCmd := -1
	curCmd := 0
	additionalReads := []*service.MemoryObject{}
	doCmd := func(ctx context.Context, cmdID api.CmdID, cmd api.Cmd) error {
		process := req.PassDefault
		typedRanges := service.TypedMemoryRanges{}
		readMemory := false
		drop := false

		if !process {
			_, process = m[cmd.CmdName()]
		}

		if process {
			cmd.Extras().Observations().ApplyReads(backup_state.Memory.ApplicationPool())
			cmd.Extras().Observations().ApplyWrites(backup_state.Memory.ApplicationPool())

			csvc, err := api.CmdToService(cmd)
			if err != nil {
				return err
			}
			comm.OnCallback(ctx, csvc)

		loop:
			for {
				resp, err := comm.GetUserResponse()
				if err != nil {
					panic(err)
					return err
				}

				switch t := resp.Req.(type) {
				case *service.StreamCommandsRequest_PassCommand:
					_ = t
					drop = false
					break loop
				case *service.StreamCommandsRequest_DropCommand:
					_ = t
					drop = true
					break loop
				case *service.StreamCommandsRequest_PutMemory:
					additionalReads = append(additionalReads, t.PutMemory.Objects...)
				case *service.StreamCommandsRequest_ResolveObject:
					v, err := resolveObject(ctx, cmd, backup_state, t.ResolveObject.Pointer, t.ResolveObject.Type.TypeIndex, t.ResolveObject.Offset)
					if err != nil {
						return err
					}

					comm.OnRequestReturn(ctx,
						&service.StreamCommandsResponse{
							Res: &service.StreamCommandsResponse_ReadObject{
								v,
							},
						},
					)
				case *service.StreamCommandsRequest_GetMemory:
					if readMemory {
						comm.OnRequestReturn(ctx,
							&service.StreamCommandsResponse{
								Res: &service.StreamCommandsResponse_TypedRanges{
									&service.TypedRangeResponse{
										Ranges: typedRanges,
									},
								},
							},
						)
						continue
					}
					readMemory = true
					// If our backup_state is behind, update it now.
					// We do this because you may want to know all of the reads
					// and writes, but NOT actually commit the command yet.
					if lastCmd < curCmd {
						for i := lastCmd + 1; i < curCmd; i++ {
							if i < len(ic) {
								ic[i].Mutate(ctx, cmdID, backup_state, nil, nil)
							} else {
								c.Commands[i-len(ic)].Mutate(ctx, cmdID, backup_state, nil, nil)
							}
						}
					}
					lastCmd = curCmd
					backup_state.Memory.SetOnCreate(func(pId memory.PoolID, pool *memory.Pool) {
						pool.OnRead = func(rng memory.Range, root uint64, tId uint64, apiId id.ID) {
							typedRanges = append(typedRanges,
								&service.TypedMemoryRange{
									Type: &path.Type{TypeIndex: tId, API: &path.API{ID: path.NewID(apiId)}},
									Range: &service.MemoryRange{
										Base: rng.Base,
										Size: rng.Size,
									},
									Root: root,
								},
							)
						}
						pool.OnWrite = func(rng memory.Range, root uint64, tId uint64, apiId id.ID) {
							typedRanges = append(typedRanges,
								&service.TypedMemoryRange{
									Type: &path.Type{TypeIndex: tId, API: &path.API{ID: path.NewID(apiId)}},
									Range: &service.MemoryRange{
										Base: rng.Base,
										Size: rng.Size,
									},
									Root: root,
								},
							)
						}
					})
					cmd.Mutate(ctx, cmdID, backup_state, nil, nil)
					backup_state.Memory.SetOnCreate(func(pId memory.PoolID, pool *memory.Pool) {
						pool.OnRead = nil
						pool.OnWrite = nil
					})
					typedRanges.Filter()
					comm.OnRequestReturn(ctx,
						&service.StreamCommandsResponse{
							Res: &service.StreamCommandsResponse_TypedRanges{
								&service.TypedRangeResponse{
									Ranges: typedRanges,
								},
							},
						},
					)

				default:
					panic(fmt.Sprintf("Unknown request %T %v", t, t))
				}
			}

		}

		curCmd++
		if !drop {
			if len(additionalReads) != 0 {
				// Start to get additional reads ready:
				// #1 Encode our data into the pool
				//    If this is a "fictional" pointer, then allocate memory
				// #2 Clone the command and add this to the list of read observations.
				// #3   Mutate
				// #4 If this WAS a fictional pointer, then free the memory as well
				resolves := make(map[uint64]api.AllocResult)
				temporaryAllocations := []api.AllocResult{}
				writes := []write{}
				// First gather all of the fictional pointers, and create
				// allocations for them
				for _, r := range additionalReads {
					if (r.Pointer.Fictional) {
						tp, err := types.GetType(r.Type.TypeIndex)
						if err != nil {
							return log.Err(ctx, err, fmt.Sprintf("Error getting type $+v", r.Type))
						}
						st, ok := tp.Ty.(*types.Type_Slice)
						if !ok {
							return log.Err(ctx, err, fmt.Sprintf("Type %+v is not slice", r.Type))
						}
						childType, err := types.GetType(st.Slice.Underlying)
						if err != nil {
							return log.Err(ctx, err, fmt.Sprintf("Could not find underlying type %+v", st.Slice.Underlying))
						}
						element_size, err := childType.SizeAlignment(ctx, state.MemoryLayout)
						if err != nil {
							return log.Err(ctx, err, fmt.Sprintf("Could not find alignment/size type %+v", childType))
						}
						nElements := len(r.WriteObject.Val.(*memory_box.Value_Slice).Slice.Values)
						res, err := state.Alloc(ctx, element_size.ByteSize * uint64(nElements))
						if err != nil {
							return log.Err(ctx, err, fmt.Sprintf("Could not find allocate memory %+v, %+v", element_size.ByteSize, uint64(nElements)))
						}
						resolves[r.Pointer.Address] = res
					}
				}

				for _, r := range additionalReads {
					tp, err := types.GetType(r.Type.TypeIndex)
					if err != nil {
						return log.Err(ctx, err, fmt.Sprintf("Could not find type%+v", r.Type))
					}

					buf := &bytes.Buffer{}
					e := memory.NewEncoder(endian.Writer(buf, state.MemoryLayout.GetEndian()), state.MemoryLayout)
					err = memory_box.EncodeMemory(ctx, func(p uint64)uint64 {
						if r, ok := resolves[p]; ok {
							return r.Range().Base
						}
						return 0
					}, state.MemoryLayout, e, tp, r.WriteObject)
					if err != nil {
						return log.Err(ctx, err, fmt.Sprintf("Could not unbox type %+v   %+v", tp, r.WriteObject))
					}
					ptr := r.Pointer.Address
					if (r.Pointer.Fictional) {
						res := resolves[r.Pointer.Address]
						ptr = res.Range().Base
						if len(buf.Bytes()) != int(res.Range().Size) {
							panic("Invalid memory box size")
						}
					}
					id, err := database.Store(ctx, buf.Bytes())
					if err != nil {
						return log.Err(ctx, err, fmt.Sprintf("Could not store bytes %+v", buf))
					}
					writes=append(writes, write{
						memory.Range{ptr, uint64(len(buf.Bytes()))},
						id,
					})
				}
				log.E(ctx, "")
				// Now that we have unboxed and created the new memory properly,
				// we can clone the command, and add our reads
				newCmd := cmd.Clone(state.Arena)
				for _, w := range writes {
					newCmd.Extras().GetOrAppendObservations().AddRead(w.rng, w.id)
				}
				err := newCmd.Mutate(ctx, cmdID, state, nil, nil)
				for _, v := range temporaryAllocations {
					v.Free()
				}
				additionalReads = additionalReads[:0:0]
				return err
			} else {
				return cmd.Mutate(ctx, cmdID, state, nil, nil)
			}
		} else {
			return nil
		}
	}

	if len(ic) > 0 {
		err = api.ForeachCmd(ctx, ic, true, doCmd)
		if err != nil {
			return err
		}
		comm.OnRequestReturn(ctx,
			&service.StreamCommandsResponse{
				Res: &service.StreamCommandsResponse_InitialCommandsDone{
					true,
				},
			},
		)
	}
	err = api.ForeachCmd(ctx, c.Commands, true, doCmd)

	return err
}
