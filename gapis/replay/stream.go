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
	"context"
	"fmt"
	"reflect"
	"sync/atomic"

	"github.com/google/gapid/core/app/status"
	"github.com/google/gapid/core/data/id"
	"github.com/google/gapid/core/log"
	"github.com/google/gapid/core/os/device/bind"
	"github.com/google/gapid/gapis/api"
	"github.com/google/gapid/gapis/capture"
	"github.com/google/gapid/gapis/memory"
	"github.com/google/gapid/gapis/messages"
	"github.com/google/gapid/gapis/resolve/initialcmds"
	"github.com/google/gapid/gapis/service"
	"github.com/google/gapid/gapis/service/memory_box"
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

func getMemory(ctx context.Context, command api.Cmd, st *api.GlobalState, base uint64, t uint64, offs uint64) (*memory_box.Value, error) {
	
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
	backup_pools := state.Memory.Clone()
	
	doCmd := func(ctx context.Context, id api.CmdID, cmd api.Cmd) error {
		process := req.PassDefault
		if !process {
			_, process = m[cmd.CmdName()]
		}

		if process {
			cmd.Extras().Observations().ApplyReads(backup_pools.ApplicationPool())
			cmd.Extras().Observations().ApplyWrites(backup_pools.ApplicationPool())
			
			c, err := api.CmdToService(cmd)
			if err != nil {
				return err
			}
			comm.OnCallback(ctx, c)

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
					break loop
				case *service.StreamCommandsRequest_GetMemory:
					old_pools := state.Memory
					state.Memory = backup_pools
					v, err := getMemory(ctx, cmd, state, t.GetMemory.Pointer, t.GetMemory.Type.TypeIndex, t.GetMemory.Offset)
					if err != nil {
						return err
					}
					state.Memory = old_pools
					comm.OnRequestReturn(ctx,
						&service.StreamCommandsResponse{
							Res: &service.StreamCommandsResponse_ReadObject{
								v,
							},
						},
					)
				default:
					panic(fmt.Sprintf("Unknown request %T %v", t, t))
				}
			}

		}
		return cmd.Mutate(ctx, id, state, nil, nil)
	}

	if len(ic) > 0 {
		err = api.ForeachCmd(ctx, ic, doCmd)
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
	err = api.ForeachCmd(ctx, c.Commands, doCmd)
	return err
}