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

package main

import (
	"context"
	"flag"
	"fmt"

	"github.com/google/gapid/core/app"
	"github.com/google/gapid/core/app/flags"
	"github.com/google/gapid/core/os/file"
	"github.com/google/gapid/gapil/resolver"
	"github.com/google/gapid/gapil/semantic"
)

func init() {
	app.AddVerb(&app.Verb{
		Name:      "gen_vulkan_layer",
		ShortHelp: "Generates a vulkan layer with the given ",
		Action:    &generateVulkanLayerVerb{},
	})
}

type generateVulkanLayerVerb struct {
	Dir        string        `help:"The output directory"`
	Tracer     string        `help:"The template function trace expression"`
	Gopath     string        `help:"the go path to use when looking up packages"`
	GlobalList flags.Strings `help:"A global value setting for the template"`
	Search     file.PathList `help:"The set of paths to search for includes"`
}

func (v *generateVulkanLayerVerb) Run(ctx context.Context, flags flag.FlagSet) error {
	api, mappings, err := resolve(ctx, v.Search, flags, resolver.Options{
		ExtractCalls:   true,
		RemoveDeadCode: true,
	})
	_ = api
	_ = mappings
	if err != nil {
		return err
	}

	var commands []*semantic.Function
	for _, function := range api.Functions {
		commands = append(commands, function)
	}
	for _, class := range api.Classes {
		for _, method := range class.Methods {
			commands = append(commands, method)
		}
	}
	for _, pseudonym := range api.Pseudonyms {
		for _, method := range pseudonym.Methods {
			commands = append(commands, method)
		}
	}

	for _, c := range commands {
		if c.GetAnnotation("synthetic") != nil ||
			c.GetAnnotation("pfn") != nil {
			continue
		}
		fmt.Printf("%+v\n", c.Name())
	}

	return nil
}
