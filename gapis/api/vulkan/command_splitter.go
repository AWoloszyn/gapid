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

package vulkan

import (
	"context"
	"fmt"

	"github.com/google/gapid/core/memory/arena"
	"github.com/google/gapid/gapis/api"
	"github.com/google/gapid/gapis/api/transform"
	"github.com/google/gapid/gapis/replay/builder"
	"github.com/google/gapid/gapis/service/path"
)

// CommandSplitter is a transform that allows vulkan command-buffers
// to be split in the middle. It inserts dummy commands into the stream
// at the given subcommand ids.
type CommandBufferInsertionCommand struct {
	cmdBuffer VkCommandBuffer
	idx       api.SubCmdIdx
	callee    api.Cmd
}

// Interface check
var _ api.Cmd = &CommandBufferInsertionCommand{}

func (*CommandBufferInsertionCommand) Mutate(context.Context, api.CmdID, *api.GlobalState, b *builder.Builder, api.StateWatcher) error {
	if (b != nil) {
		return fmt.Errorf("This command should have been replaced replaced before it got to the builder")
	}
	return nil
}

func (s *CommandBufferInsertionCommand) Caller() api.CmdID {
	return s.callee.Caller()
}

func (s *CommandBufferInsertionCommand) SetCaller(c api.CmdID) {
	s.callee.SetCaller(c)
}

func (s *CommandBufferInsertionCommand) Thread() uint64 {
	return s.callee.Thread()
}

func (s *CommandBufferInsertionCommand) SetThread(c uint64) {
	s.callee.SetThread(c)
}

// CmdName returns the name of the command.
func (s *CommandBufferInsertionCommand) CmdName() string {
	return "CommandBufferInsertion"
}

func (s *CommandBufferInsertionCommand) CmdParams() api.Properties {
	return api.Properties{}
}

func (s *CommandBufferInsertionCommand) CmdResult() *api.Property {
	return nil
}

func (s *CommandBufferInsertionCommand) CmdFlags(context.Context, api.CmdID, *api.GlobalState) api.CmdFlags {
	return 0
}

func (s *CommandBufferInsertionCommand) Extras() *api.CmdExtras {
	return nil
}

func (s *CommandBufferInsertionCommand) Clone(a arena.Arena) api.Cmd {
	return &CommandBufferInsertionCommand{
		s.cmdBuffer,
		s.idx,
		s.callee.Clone(a),
	}
}

func (s *CommandBufferInsertionCommand) Alive() bool {
	return true
}

func (s *CommandBufferInsertionCommand) Terminated() bool {
	return true
}

func (s *CommandBufferInsertionCommand) SetTerminated(bool) {
}

func (s *CommandBufferInsertionCommand) API() api.API {
	return s.callee.API()
}


func (s *CommandSplitter) MustAllocReadDataForSubmit(g *api.GlobalState, v ...interface{}) api.AllocResult {
	allocateResult := g.AllocDataOrPanic(sb.ctx, v...)
	s.readMemoriesForSubmit = append(s.readMemoriesForSubmit, &allocateResult)
	rng, id := allocateResult.Data()
	g.Memory.ApplicationPool().Write(rng.Base, memory.Resource(id, rng.Size))
	return allocateResult
}

func (s *CommandSplitter) MustAllocReadDataForCmd(g *api.GlobalState, v ...interface{}) api.AllocResult {
	allocateResult := g.AllocDataOrPanic(sb.ctx, v...)
	s.readMemoriesForCmd = append(s.readMemoriesForCmd, &allocateResult)
	rng, id := allocateResult.Data()
	g.Memory.ApplicationPool().Write(rng.Base, memory.Resource(id, rng.Size))
	return allocateResult
}

func (s *CommandSplitter) MustAllocWriteDataForCmd(g *api.GlobalState, v ...interface{}) api.AllocResult {
	allocateResult := g.AllocDataOrPanic(sb.ctx, v...)
	s.writeMemoriesForCmd = append(s.writeMemoriesForCmd, &allocateResult)
	return allocateResult
}

func (s *CommandSplitter) WriteCommand(ctx context.Context, g api.Cmd, out transform.Writer) {
	for s := range(t.readMemoriesForCmd) {
		g.AddRead(t.readMemoriesForCmd[i].Data())
	}
	for s := range(t.writeMemoriesForCmd) {
		g.AddWrite(t.writeMemoriesForCmd[i].Data())
	}
	t.readMemoriesForCmd = []*api.AllocResult{}
	t.writeMemoriesForCmd = []*api.AllocResult{}
	out.MutateAndWrite(ctx, g.ID(), out)
}


// This does NOT handle the event case where we have a signal after submit.
// That case should be removed.
type CommandSplitter struct {
	lastRequest      api.SubCmdIdx
	requestsSubIndex []api.SubCmdIdx

	readMemoriesForSubmit []*api.AllocResult
	readMemoriesForCmd    []*api.AllocResult
	writeMemoriesForCmd   []*api.AllocResult
	VkCommandPool         pool
}

func NewCommandSplitter(ctx context.Context, capture *path.Capture) (*CommandSplitter, error) {
	return &CommandSplitter{api.SubCmdIdx{}, make([]api.SubCmdIdx, 0),
		make([]*api.AllocResult), make([]*api.AllocResult),
		0}, nil
}

// Add adds the command with identifier id to the set of commands that will be split.
func (t *CommandSplitter) Split(ctx context.Context, extraCommands int, id api.SubCmdIdx) error {
	t.requestsSubIndex = append(t.requestsSubIndex, append(api.SubCmdIdx{}, id...))
	if t.lastRequest.LessThan(id) {
		t.lastRequest = append(api.SubCmdIdx{}, id...)
	}

	return nil
}

func (t* CommandSplitter) getCommandPool(ctx context.Context, queueSubmit *VkQueueSubmit, out transform.Writer) VkCommandPool {
	if (t.pool != 0) {
		return t.pool
	}
	s := out.State()
	a := s.Arena
	l := s.MemoryLayout()
	cb := CommandBuilder{Thread: a.Thread(), Arena: s.Arena}
	vulkan_state := GetState(s)
	queue := vulkan_state.Queues.Get(queueSubmit.Queue())
	t.pool := VkCommandPool(newUnusedID(false, func(x uint64) bool { ok := GetState(s).CommandPools().Contains(VkCommandPool(x)); return ok }))

	poolCreateInfo := NewVkCommandPoolCreateInfo(a,
		VkStructureType_VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,                                 // sType
		NewVoidᶜᵖ(memory.Nullptr),                                                                  // pNext
		VkCommandPoolCreateFlags(VkCommandPoolCreateFlagBits_VK_COMMAND_POOL_CREATE_TRANSIENT_BIT), // flags
		queue.Family(), // queueFamilyIndex
	)

	t.WriteCommand(ctx, cb.VkCreateCommandPool(
		vkDevice,
		t.MustAllocReadDataForCmd(s, poolCreateInfo).Ptr(),
		memory.Nullptr,
		t.MustAllocWriteDataForCmd(s, t.pool).Ptr(),
		VkResult_VK_SUCCESS,
	), out)
	return t.pool
}

func (t* CommandSplitter) getStartedCommandBuffer(ctx context.Context, queueSubmit* VkQueueSubmit, out transform.Writer) VkCommandBuffer {
	s := out.State()
	a := s.Arena
	cb := CommandBuilder{Thread: a.Thread(), Arena: a}

	commandPoolID := getCommandPool(ctx, queueSubmit, out)

	commandBufferAllocateInfo := NewVkCommandBufferAllocateInfo(a,
		VkStructureType_VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO, // sType
		NewVoidᶜᵖ(memory.Nullptr),                                      // pNext
		commandPoolID,                                                  // commandPool
		VkCommandBufferLevel_VK_COMMAND_BUFFER_LEVEL_PRIMARY, // level
		1, // commandBufferCount
	)
	commandBufferID := VkCommandBuffer(newUnusedID(true, func(x uint64) bool { ok := GetState(s).CommandBuffers().Contains(VkCommandBuffer(x)); return ok }))

	t.WriteCommand(ctx,
		cb.VkAllocateCommandBuffers(
			vkDevice,
			t.MustAllocReadDataForCmd(s, commandBufferAllocateInfo).Ptr(),
			t.MustAllocWriteDataForCmd(s, commandBufferData).Ptr(),
			VkResult_VK_SUCCESS,
		), out)

	commandBufferBegin := NewVkCommandBufferBeginInfo(a,
		VkStructureType_VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, // sType
		NewVoidᶜᵖ(memory.Nullptr),                                   // pNext
		0,                                                           // flags
		NewVkCommandBufferInheritanceInfoᶜᵖ(memory.Nullptr),         // pInheritanceInfo
	)
	t.WriteCommand(ctx,
		cb.VkBeginCommandBuffer(
			commandBufferID,
			t.MustAllocReadDataForCmd(s,commandBufferBegin).Ptr(),
		), out)

	return commandBufferID
}

func (t* CommandSplitter) splitCommandBuffer(ctx context.Context, commandBuffer CommandBufferObjectʳ, queueSubmit *VkQueueSubmit, id api.SubCmdIdx,  cuts []api.SubCmdIdx, out transform.Writer) VkCommandBuffer {
	s := out.State()
	a := s.Arena
	cb := CommandBuilder{Thread: a.Thread(), Arena: a}
	newCommandBuffer := getStartedCommandBuffer(ctx, queueSubmit, out)
	for i := 0; i < commandBuffer.CommandReferences().Len(); i++ {
		cr := commandBuffer.CommandReferences().Get(uint32(i))
		args := GetCommandArgs(ctx, cr, st)
		switch ar := args.(type) {
			case VkCmdBeginRenderPassArgsʳ:
		}
	}

	t.WriteCommand(ctx,
		cb.VkEndCommandBuffer(newCommandBuffer))
}

func (t* CommandSplitter) splitSubmit(ctx context.Context, submit VkSubmitInfo, id []api.SubCmdIdx, queueSubmit *VkQueueSubmit, out transform.Writer) VkSubmitInfo {
	return VkSubmitInfo{}
}

func (t* CommandSplitter) splitAfterSubmit(ctx context.Context, id []api.SubCmdIdx, queueSubmit *VkQueueSubmit, out transform.Writer) VkSubmitInfo {
	s := out.State()
	a := s.Arena

	commandBuffer := getStartedCommandBuffer(ctx, queueSubmit, out)
	out.MutateAndWrite(ctx, id[0], &CommandBufferInsertionCommand{
		VkCommandBuffer(0),
		id,
		queueSubmit,
	})
	t.WriteCommand(ctx,
		cb.VkEndCommandBuffer(commandBuffer))

	info := NewVkSubmitInfo(a,
		VkStructureType_VK_STRUCTURE_TYPE_SUBMIT_INFO, // sType
		NewVoidᶜᵖ(memory.Nullptr),                                   // pNext
		0, // waitSemaphoreCount,
		VkSemaphoreᶜᵖ(memory.Nullptr),                                   // pWaitSemaphores
		VkPipelineStageFlagsᶜᵖ(memory.Nullptr), // pWaitDstStageMask
		1, // commandBufferCount
		cb.MustAllocReadDataForSubmit(commandBuffer).Ptr(),
		0,	// signalSemaphoreCount
		VkSemaphoreᶜᵖ(memory.Nullptr),                                   // pSignalSemaphores
	)

	return info
}

func (t* CommandSplitter) rewriteQueueSubmit(ctx context.Context, id api.Cmd, cuts []api.SubCmdIdx, queueSubmit *VkQueueSubmit, out transform.Writer) {
	s := out.State()
	c := GetState(s)
	l := s.MemoryLayout()
	cb := CommandBuilder{Thread: a.Thread(), Arena: s.Arena}
	queueSubmit.Extras().Observations().ApplyReads(s.Memory.ApplicationPool())

	submitInfos := queueSubmit.PSubmits().Slice(0, uint64(a.SubmitCount()), l)
	newSubmitInfos := []VkSubmitInfo{}

	newSubmit := cb.VkQueueSubmit(queueSubmit.Queue(), queueSubmit.SubmitCount(), queueSubmit.PSubmits(), queueSubmit.Fence(), queueSubmit.Result())
	newSubmit.Extras().MustClose(queueSubmit.Extras().All()...)

	for i := 0; i < len(submitInfos); i++ {
		newCuts := []api.SubCmdIdx{}
		addAfterSubmit := false
		for s := range(cuts) {
			if s[0] == i {
				if (len(s) == 1) {
					addAfterSubmit = true
				} else {
					newCuts = append(newCuts, s[1:])
				}
			}
		}
		newSubmitInfo := submitInfos[i]
		if len(newCuts) != 0 {
			newSubmitInfo = t.splitSubmit(ctx, api.SubCmdIdx{id, i}, newCuts, out transform.Writer)
		}
		newSubmitInfos = append(newSubmitInfos, newSubmitInfo)
		if addAfterSubmit {
			newSubmitInfos = append(newSubmitInfos, t.splitAfterSubmit(ctx, submitInfos[i], []api.SubCmdIdx{id, i}), out transform.Writer)
		}
	}
	newSubmit.SetSubmitCount(len(newSubmitInfos))
	newSubmit.SetPSubmits(NewVkSubmitInfoᶜᵖ(t.MustAllocReadDataForSubmit(s, newSubmitInfos).Ptr()))

	for x := range(t.readMemoriesForSubmit) {
		newSubmit.AddRead(t.readMemoriesForSubmit[i].Data())
	}
	t.readMemoriesForSubmit = []*api.AllocResult{}
	out.MutateAndWrite(ctx, id, newSubmit)
}

func (t *CommandSplitter) Transform(ctx context.Context, id api.CmdID, cmd api.Cmd, out transform.Writer) {
	inRange := false
	var topCut api.SubCmdIdx
	cuts := []api.SubCmdIdx{}
	thisID := api.SubCmdIdx{uint64(id)}
	for _, r := range t.requestsSubIndex {
		if thisID.Contains(r) {
			inRange = true
			if thisID.Equals(r) {
				topCut = r
			} else {
				cuts = append(cuts, r[1:]...)
			}
		}
	}

	if !inRange {
		out.MutateAndWrite(ctx, id, cmd)
		return
	}

	if len(cuts) == 0 {
		out.MutateAndWrite(ctx, id, cmd)
		out.MutateAndWrite(ctx, id, &CommandBufferInsertionCommand{
			VkCommandBuffer(0),
			topCut,
			cmd,
		})
		return
	}

	// Actually do the cutting here:
	queueSubmit, ok := cmd.(*VkQueueSubmit)
	// If this is not a queue submit it has no business having
	// subcommands.
	if !ok {
		out.MutateAndWrite(ctx, id, thisCmd)
		return
	}
	// TODO(awoloszyn): Validate the queue actually exists

	thisCmd := t.rewriteQueueSubmit(ctx, id, queueSubmit, out)
	out.MutateAndWrite(ctx, id, thisCmd)
	if len(topCut) == 0 {
		return
	}
	out.MutateAndWrite(ctx, id, &CommandBufferInsertionCommand{
		VkCommandBuffer(0),
		topCut,
		cmd,
	})
}

func (t *CommandSplitter) Flush(ctx context.Context, out transform.Writer)       {}
func (t *CommandSplitter) PreLoop(ctx context.Context, output transform.Writer)  {}
func (t *CommandSplitter) PostLoop(ctx context.Context, output transform.Writer) {}
func (t *CommandSplitter) BuffersCommands() bool                                 { return false }
