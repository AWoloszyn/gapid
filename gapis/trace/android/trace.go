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

package android

import (
	"archive/zip"
	"context"
	"fmt"
	"io"
	"io/ioutil"
	"os"
	"path/filepath"
	"regexp"
	"strings"
	"time"

	"github.com/google/gapid/core/log"
	"github.com/google/gapid/core/os/android"
	"github.com/google/gapid/core/os/android/adb"
	"github.com/google/gapid/core/os/android/apk"
	"github.com/google/gapid/core/os/device/bind"
	"github.com/google/gapid/gapidapk"
	"github.com/google/gapid/gapidapk/pkginfo"
	gapii "github.com/google/gapid/gapii/client"
	"github.com/google/gapid/gapis/trace/tracer"
)

// Only update the package list every 30 seconds at most
var packageUpdateTime = 30.0

type AndroidTracer struct {
	b                    adb.Device
	packages             *pkginfo.PackageList
	lastIconDensityScale float32
	lastPackageUpdate    time.Time
}

func (t *AndroidTracer) GetPackages(ctx context.Context, isRoot bool, iconDensityScale float32) (*pkginfo.PackageList, error) {
	refresh := time.Since(t.lastPackageUpdate).Seconds() > packageUpdateTime ||
		t.lastIconDensityScale != iconDensityScale

	if t.packages != nil && !isRoot {
		refresh = false
	}
	if refresh {
		packages, err := gapidapk.PackageList(ctx, t.b, true, iconDensityScale)
		if err != nil {
			return nil, err
		}
		pkgList := &pkginfo.PackageList{
			Packages: []*pkginfo.Package{},
			Icons: packages.Icons,
			OnlyDebuggable: packages.OnlyDebuggable,
		}

		for _, p := range packages.Packages {
			for _, activity := range p.Activities {
				if len(activity.Actions) > 0 {
					pkgList.Packages = append(pkgList.Packages, p)
					break
				}
			}
		}

		t.packages = pkgList
		t.lastPackageUpdate = time.Now()
		t.lastIconDensityScale = iconDensityScale
	}
	return t.packages, nil
}

func NewTracer(dev bind.Device) *AndroidTracer {
	return &AndroidTracer{dev.(adb.Device), nil, 1.0, time.Time{}}
}

// IsServerLocal returns true if all paths on this device can be server-local
func (t *AndroidTracer) IsServerLocal() bool {
	return false
}

func (t *AndroidTracer) CanSpecifyCWD() bool {
	return false
}

func (t *AndroidTracer) CanSpecifyEnv() bool {
	return false
}

func (t *AndroidTracer) CanUploadApplication() bool {
	return true
}

func (t *AndroidTracer) HasCache() bool {
	return true
}

func (t *AndroidTracer) CanUsePortFile() bool {
	return false
}

func (t *AndroidTracer) APITraceOptions(ctx context.Context) []tracer.APITraceOptions {
	options := make([]tracer.APITraceOptions, 0, 2)
	if t.b.Instance().Configuration.Drivers.Opengl.Version != "" {
		options = append(options, tracer.GlesTraceOptions())
	}
	if len(t.b.Instance().Configuration.Drivers.Vulkan.PhysicalDevices) > 0 {
		options = append(options, tracer.VulkanTraceOptions())
	}
	return options
}

func (t *AndroidTracer) GetTraceTargetNode(ctx context.Context, uri string, iconDensity float32) (*tracer.TraceTargetTreeNode, error) {
	packages, err := t.GetPackages(ctx, uri == "", iconDensity)

	if err != nil {
		return nil, err
	}
	if uri == "" {
		r := &tracer.TraceTargetTreeNode{
			"",
			[]byte{},
			"",
			"",
			[]string{},
			"",
			"",
			"",
		}
		for _, x := range packages.Packages {
			r.Children = append(r.Children, x.Name)
		}
		return r, nil
	}

	requestedPkg := uri

	intent := ""
	if strings.Contains(requestedPkg, ":") {
		intent = requestedPkg[0:strings.Index(requestedPkg, ":")]
		requestedPkg = requestedPkg[strings.Index(requestedPkg, ":")+1:]
	}

	activity := ""
	pkg := ""
	if strings.Contains(requestedPkg, "/") {
		ap := strings.SplitN(requestedPkg, "/", 2)
		pkg = ap[0]
		activity = ap[1]
	} else {
		pkg = requestedPkg
	}

	inst_pkg := packages.FindByName(pkg)
	if inst_pkg == nil {
		return nil, log.Errf(ctx, nil, "Could not find package %s", pkg)
	}

	if activity != "" {
		var act *pkginfo.Activity
		for _, a := range inst_pkg.Activities {
			if a.Name == activity {
				act = a
				break
			}
		}
		if act == nil {
			return nil, log.Errf(ctx, nil, "Could not find activity %s, in package %s", activity, pkg)
		}

		if intent != "" {
			for _, i := range act.Actions {
				if i.Name == intent {
					return &tracer.TraceTargetTreeNode{
						intent,
						packages.GetIcon(inst_pkg),
						fmt.Sprintf("%s:%s/%s", intent, pkg, activity),
						fmt.Sprintf("%s:%s/%s", intent, pkg, activity),
						[]string{},
						fmt.Sprintf("%s/%s", pkg, activity),
						pkg,
						"",
					}, nil
				}
			}
			return nil, log.Errf(ctx, nil, "Could not find Intent %s, in package %s/%s", intent, pkg, activity)
		}
		r := &tracer.TraceTargetTreeNode{
			activity,
			packages.GetIcon(inst_pkg),
			fmt.Sprintf("%s/%s", pkg, activity),
			"",
			[]string{},
			pkg,
			pkg,
			"",
		}
		var defaultAction *pkginfo.Action
		for _, i := range act.Actions {
			r.Children = append(r.Children, fmt.Sprintf("%s:%s/%s", i.Name, pkg, activity))
			if i.IsLaunch {
				defaultAction = i
			}
		}
		if len(act.Actions) == 1 {
			defaultAction = act.Actions[0]
		}
		if defaultAction != nil {
			r.TraceURI = fmt.Sprintf("%s:%s/%s", defaultAction.Name, pkg, activity)
		}
		return r, nil
	}

	default_action := ""
	r := &tracer.TraceTargetTreeNode{
		pkg,
		packages.GetIcon(inst_pkg),
		pkg,
		"",
		[]string{},
		"",
		pkg,
		"",
	}
	for _, a := range inst_pkg.Activities {
		if len(a.Actions) > 0 {
			r.Children = append(r.Children, fmt.Sprintf("%s/%s", pkg, a.Name))
			for _, act := range a.Actions {
				if act.IsLaunch {
					default_action = fmt.Sprintf("%s:%s/%s", act.Name, pkg, a.Name)
				}
			}
		}
	}

	if len(inst_pkg.Activities) == 1 {
		if len(inst_pkg.Activities[0].Actions) == 1 {
			default_action = fmt.Sprintf("%s:%s/%s", inst_pkg.Activities[0].Actions[0].Name, pkg, inst_pkg.Activities[0].Name)
		}
	}
	r.TraceURI = default_action
	return r, nil
}

// InstallPackage installs the given package onto the android device.
// If it is a zip file that contains an apk and an obb file
// then we install them seperately.
// Returns a function used to clean up the package and obb
func (t *AndroidTracer) InstallPackage(ctx context.Context, o *tracer.TraceOptions) (*android.InstalledPackage, func(), error) {
	tempDir, err := ioutil.TempDir("", "")
	if err != nil {
		return nil, nil, err
	}
	defer os.RemoveAll(tempDir)
	// Call it .apk now, because it may actually be our apk
	zipName := filepath.Join(tempDir + "zip.apk")
	apkName := zipName
	obbName := ""

	if err = ioutil.WriteFile(zipName, o.UploadApplication, os.FileMode(0600)); err != nil {
		return nil, nil, err
	}

	r, err := zip.OpenReader(zipName)
	defer r.Close()
	if err != nil {
		return nil, nil, err
	}
	hasObb := false
	if len(r.File) == 2 {
		if (strings.HasSuffix(r.File[0].Name, ".apk") &&
			strings.HasSuffix(r.File[1].Name, ".obb")) ||
			(strings.HasSuffix(r.File[1].Name, ".apk") &&
				strings.HasSuffix(r.File[0].Name, ".obb")) {
			hasObb = true
		}
	}

	if hasObb {
		// We should extract the .zip file into a .apk and a .obb file
		apkName = filepath.Join(tempDir, "a.apk")
		obbName = filepath.Join(tempDir, "a.obb")

		apkFile, err := os.OpenFile(apkName, os.O_RDWR|os.O_CREATE, 0600)
		if err != nil {
			return nil, nil, err
		}
		obbFile, err := os.OpenFile(obbName, os.O_RDWR|os.O_CREATE, 0600)
		if err != nil {
			apkFile.Close()
			return nil, nil, err
		}

		for i := 0; i < 2; i++ {
			if f, err := r.File[i].Open(); err == nil {
				if strings.HasSuffix(r.File[i].Name, ".apk") {
					io.Copy(apkFile, f)
				} else {
					io.Copy(obbFile, f)
				}
				f.Close()
			}

		}
		apkFile.Close()
		obbFile.Close()
	}

	apkData, err := ioutil.ReadFile(apkName)
	if err != nil {
		return nil, nil, log.Err(ctx, err, "Could not read apk file")
	}
	info, err := apk.Analyze(ctx, apkData)
	if err != nil {
		return nil, nil, log.Err(ctx, err, "Could not analyze apk file, not an APK?")
	}

	if err := t.b.InstallAPK(ctx, apkName, true, true); err != nil {
		return nil, nil, log.Err(ctx, err, "Failed to install APK")
	}

	pkg := &android.InstalledPackage{
		Name:        info.Name,
		Device:      t.b,
		ABI:         t.b.Instance().GetConfiguration().PreferredABI(info.ABI),
		Debuggable:  info.Debuggable,
		VersionCode: int(info.VersionCode),
		VersionName: info.VersionName,
	}
	cleanup := func() {}
	if obbName != "" {
		if err := pkg.PushOBB(ctx, obbName); err != nil {
			pkg.Uninstall(ctx)
			return nil, nil, log.Err(ctx, err, "Pushing OBB failed")
		}
		cleanup = func() {
			pkg.Uninstall(ctx)
			pkg.RemoveOBB(ctx)
		}
		if err = pkg.GrantExternalStorageRW(ctx); err != nil {
			log.W(ctx, "Failed to grant OBB read/write permission, (app likely already has them). Ignoring: %s", err)
		}
	} else {
		cleanup = func() {
			pkg.Uninstall(ctx)
		}
	}
	return pkg, cleanup, nil
}

func (t *AndroidTracer) getAction(ctx context.Context, pattern string) (string, error) {
	re := regexp.MustCompile("(?i)" + pattern)
	packages, err := t.GetPackages(ctx, pattern == "", t.lastIconDensityScale)
	if err != nil {
		return "", err
	}
	if len(packages.Packages) == 0 {
		return "", fmt.Errorf("No packages found")
	}
	matchingActions := []string{}
	for _, p := range packages.Packages {
		for _, activity := range p.Activities {
			for _, action := range activity.Actions {
				uri := fmt.Sprintf("%s:%s/%s", action.Name, p.Name, activity.Name)
				if re.MatchString(uri) {
					matchingActions = append(matchingActions, uri)
				}
			}
		}
	}
	if len(matchingActions) == 0 {
		return "", fmt.Errorf("No actions matching %s found", pattern)
	} else if len(matchingActions) > 1 {
		pkgs := fmt.Sprintf("Matching actions:\n")
		for _, test := range matchingActions {
			pkgs += fmt.Sprintf("    ")
			pkgs += fmt.Sprintf("%s\n", test)
		}
		return "", fmt.Errorf("Multiple actions matching %q found: \n%s", pattern, pkgs)
	}
	return matchingActions[0], nil
}

func (t *AndroidTracer) FindTraceTarget(ctx context.Context, str string) (*tracer.TraceTargetTreeNode, error) {
	uri, err := t.getAction(ctx, str)
	if err != nil {
		return nil, err
	}

	return t.GetTraceTargetNode(ctx, uri, t.lastIconDensityScale)
}

func (t *AndroidTracer) SetupTrace(ctx context.Context, o *tracer.TraceOptions) (*gapii.Process, func(), error) {
	var err error
	cleanup := func() {}
	var pkg *android.InstalledPackage
	var a *android.ActivityAction
	ret := &gapii.Process{}
	if len(o.UploadApplication) > 0 {
		pkg, cleanup, err = t.InstallPackage(ctx, o)
		if err != nil {
			cleanup()
			return ret, nil, err
		}
	}

	// Find the package by URI
	re := regexp.MustCompile("([^:]*):([^/]*)/\\.?(.*)")
	match := re.FindStringSubmatch(o.URI)

	if len(match) == 4 {
		if err != nil {
			return ret, nil, err
		}
		packages, err := t.b.InstalledPackages(ctx)
		if err != nil {
			return ret, nil, err
		}
		pkg = packages.FindByName(match[2])
		a = pkg.ActivityActions.FindByName(match[1], match[3])
		if a == nil {
			lines := make([]string, len(pkg.ActivityActions))
			for i, a := range pkg.ActivityActions {
				lines[i] = a.String()
			}
			cleanup()
			return ret, nil, fmt.Errorf("Action '%v:%v' not found. All package actions:\n  %v",
				match[1], match[3],
				strings.Join(lines, "\n  "))
		}
	} else {
		return ret, nil, fmt.Errorf("Could not find package matching %s", o.URI)
	}

	if !pkg.Debuggable {
		err = t.b.Root(ctx)
		switch err {
		case nil:
		case adb.ErrDeviceNotRooted:
			cleanup()
			return ret, nil, err
		default:
			cleanup()
			return ret, nil, fmt.Errorf("Failed to restart ADB as root: %v", err)
		}
		log.I(ctx, "Device is rooted")
	}

	if o.ClearCache {
		log.I(ctx, "Clearing package cache")
		if err := pkg.ClearCache(ctx); err != nil {
			return ret, nil, err
		}
	}

	if wasScreenOn, _ := t.b.IsScreenOn(ctx); !wasScreenOn {
		oldcleanup := cleanup
		cleanup = func() {
			oldcleanup()
			t.b.TurnScreenOff(ctx)
		}
	}
	log.I(ctx, "Starting with options %+v", o.GapiiOptions())
	process, err := gapii.Start(ctx, pkg, a, o.GapiiOptions())
	if err != nil {
		cleanup()
		return ret, nil, err
	}

	return process, cleanup, nil
}
