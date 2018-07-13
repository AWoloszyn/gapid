// Copyright (C) 2018 Google Inc.
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

package trace

import (
	"context"
	"os"
	"path/filepath"
	"time"

	"github.com/google/gapid/core/event/task"
	"github.com/google/gapid/core/log"
	"github.com/google/gapid/gapis/service/path"
	"github.com/google/gapid/gapis/trace/tracer"
)

func Trace(ctx context.Context, device *path.Device, start task.Signal, options *tracer.TraceOptions, written *int64) error {
	mgr := GetManager(ctx)
	if device  == nil {
		return log.Errf(ctx, nil, "Invalid device path")
	}
	tracer, ok := mgr.tracers[device.ID.ID()]
	if !ok {
		return log.Errf(ctx, nil, "Could not find tracer for device %d", device.ID.ID())
	}

	process, cleanup, err := tracer.SetupTrace(ctx, options)
	if err != nil {
		return log.Errf(ctx, err, "Could not start trace")
	}
	defer cleanup()

	os.MkdirAll(filepath.Dir(options.WriteFile), 0755)
	file, err := os.Create(options.WriteFile)
	if err != nil {
		return err
	}

	defer file.Close()

	if options.Duration > 0 {
		ctx, _ = task.WithTimeout(ctx, time.Duration(options.Duration)*time.Second)
	}

	_, err = process.Capture(ctx, start, file, written)
	return err
}

type TraceConfig struct {
	ServerLocalPath bool // Are the paths server-local for this tracer
	CanSpecifyCwd bool // Does it make sense to specify a CWD for this device
	CanUploadApplication bool // Does this device support app upload
	CanSpecifyEnv bool // Does this device support environment variables
	HasCache bool // Does this device have a clearable cache
	Apis []tracer.APITraceOptions // API specific tracing options
}

func TraceConfiguration(ctx context.Context, device *path.Device) (*TraceConfig, error) {
	mgr := GetManager(ctx)
	tracer, ok := mgr.tracers[device.ID.ID()]
	if !ok {
		return nil, log.Errf(ctx, nil, "Could not find tracer for device %d", device.ID.ID())
	}

	opts := tracer.APITraceOptions(ctx)

	config := &TraceConfig{
		ServerLocalPath: tracer.IsServerLocal(),
		CanSpecifyCwd: tracer.CanSpecifyCWD(),
		CanUploadApplication: tracer.CanUploadApplication(),
		HasCache: tracer.HasCache(),
		CanSpecifyEnv: tracer.CanSpecifyEnv(),
		Apis: opts,
	}

	return config, nil
}
