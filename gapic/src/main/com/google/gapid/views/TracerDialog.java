/*
 * Copyright (C) 2017 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package com.google.gapid.views;

import static com.google.gapid.widgets.Widgets.createBoldLabel;
import static com.google.gapid.widgets.Widgets.createCheckbox;
import static com.google.gapid.widgets.Widgets.createComposite;
import static com.google.gapid.widgets.Widgets.createDropDownViewer;
import static com.google.gapid.widgets.Widgets.createLabel;
import static com.google.gapid.widgets.Widgets.createSpinner;
import static com.google.gapid.widgets.Widgets.createTextarea;
import static com.google.gapid.widgets.Widgets.createTextbox;
import static com.google.gapid.widgets.Widgets.ifNotDisposed;
import static com.google.gapid.widgets.Widgets.withIndents;
import static com.google.gapid.widgets.Widgets.withLayoutData;
import static com.google.gapid.widgets.Widgets.withMargin;

import com.google.common.base.Throwables;
import com.google.gapid.models.Analytics.View;
import com.google.gapid.models.Devices;
import com.google.gapid.models.Devices.DeviceCaptureInfo;
import com.google.gapid.models.Models;
import com.google.gapid.models.Settings;
import com.google.gapid.models.TraceTargets;
import com.google.gapid.proto.device.Device;
import com.google.gapid.proto.service.Service.ClientAction;
import com.google.gapid.proto.service.Service.DeviceAPITraceConfiguration;
import com.google.gapid.proto.service.Service.DeviceTraceConfiguration;
import com.google.gapid.proto.service.Service.TraceTargetTreeNode;
import com.google.gapid.server.Client;
import com.google.gapid.server.Tracer;
import com.google.gapid.server.Tracer.TraceRequest;
import com.google.gapid.util.Messages;
import com.google.gapid.util.Scheduler;
import com.google.gapid.widgets.ActionTextbox;
import com.google.gapid.widgets.DialogBase;
import com.google.gapid.widgets.FileTextbox;
import com.google.gapid.widgets.LoadingIndicator;
import com.google.gapid.widgets.Theme;
import com.google.gapid.widgets.Widgets;

import org.eclipse.jface.dialogs.IDialogConstants;
import org.eclipse.jface.viewers.ArrayContentProvider;
import org.eclipse.jface.viewers.ComboViewer;
import org.eclipse.jface.viewers.LabelProvider;
import org.eclipse.jface.viewers.StructuredSelection;
import org.eclipse.jface.window.Window;
import org.eclipse.swt.SWT;
import org.eclipse.swt.layout.GridData;
import org.eclipse.swt.layout.GridLayout;
import org.eclipse.swt.widgets.Button;
import org.eclipse.swt.widgets.Composite;
import org.eclipse.swt.widgets.Control;
import org.eclipse.swt.widgets.DirectoryDialog;
import org.eclipse.swt.widgets.Event;
import org.eclipse.swt.widgets.FileDialog;
import org.eclipse.swt.widgets.Label;
import org.eclipse.swt.widgets.Listener;
import org.eclipse.swt.widgets.Shell;
import org.eclipse.swt.widgets.Spinner;
import org.eclipse.swt.widgets.TabFolder;
import org.eclipse.swt.widgets.Text;

import java.io.File;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Optional;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicBoolean;
import java.util.logging.Logger;

/**
 * Dialogs used for capturing a trace.
 */
public class TracerDialog {
  private static final Logger LOG = Logger.getLogger(TracerDialog.class.getName());

  private TracerDialog() {
  }

  public static void showOpenTraceDialog(Shell shell, Models models) {
    models.analytics.postInteraction(View.Main, ClientAction.Open);
    FileDialog dialog = new FileDialog(shell, SWT.OPEN);
    dialog.setFilterNames(new String[] { "Trace Files (*.gfxtrace)", "All Files" });
    dialog.setFilterExtensions(new String[] { "*.gfxtrace", "*" });
    dialog.setFilterPath(models.settings.lastOpenDir);
    String result = dialog.open();
    if (result != null) {
      models.capture.loadCapture(new File(result));
    }
  }

  public static void showSaveTraceDialog(Shell shell, Models models) {
    models.analytics.postInteraction(View.Main, ClientAction.Save);
    FileDialog dialog = new FileDialog(shell, SWT.SAVE);
    dialog.setFilterNames(new String[] { "Trace Files (*.gfxtrace)", "All Files" });
    dialog.setFilterExtensions(new String[] { "*.gfxtrace", "*" });
    dialog.setFilterPath(models.settings.lastOpenDir);
    String result = dialog.open();
    if (result != null) {
      models.capture.saveCapture(new File(result));
    }
  }

  public static void showTracingDialog(Client client, Shell shell, Models models, Widgets widgets) {
    models.analytics.postInteraction(View.Trace, ClientAction.Show);
    TraceInputDialog input =
        new TraceInputDialog(shell, models, widgets, models.devices::loadDevices);
    if (loadDevicesAndShowDialog(input, models) == Window.OK) {
      TraceProgressDialog progress = new TraceProgressDialog(shell, input.getValue(), widgets.theme);
      AtomicBoolean failed = new AtomicBoolean(false);
      Tracer.Trace trace = Tracer.trace(client,
          shell.getDisplay(), models.settings, input.getValue(), new Tracer.Listener() {
        @Override
        public void onProgress(String message) {
          progress.append(message);
        }

        @Override
        public void onFailure(Throwable error) {
          progress.append("Tracing failed:");
          progress.append(Throwables.getStackTraceAsString(error));
          failed.set(true);
        }
      });
      progress.setOnStart(trace::start);
      progress.open();
      trace.stop();
      if (!failed.get()) {
        models.capture.loadCapture(input.getValue().output);
      }
    }
  }

  private static int loadDevicesAndShowDialog(TraceInputDialog dialog, Models models) {
    Devices.Listener listener = new Devices.Listener() {
      @Override
      public void onCaptureDevicesLoaded() {
        dialog.setDevices(models.devices.getCaptureDevices());
      }
    };
    models.devices.addListener(listener);
    try {
      models.devices.loadDevices();
      return dialog.open();
    } finally {
      models.devices.removeListener(listener);
    }
  }

  /**
   * Dialog to request the information from the user to start a trace (which app, filename, etc.).
   */
  private static class TraceInputDialog extends DialogBase {
    private final Models models;
    private final Widgets widgets;
    private final Runnable refreshDevices;

    private TabFolder folder;
    private SharedTraceInput traceInput;
    private List<DeviceCaptureInfo> devices;

    private Tracer.TraceRequest value;

    public TraceInputDialog(Shell shell, Models models, Widgets widgets, Runnable refreshDevices) {
      super(shell, widgets.theme);
      this.models = models;
      this.widgets = widgets;
      this.refreshDevices = refreshDevices;
    }

    public Tracer.TraceRequest getValue() {
      return value;
    }

    @Override
    public String getTitle() {
      return Messages.CAPTURE_TRACE;
    }

    @Override
    protected Control createDialogArea(Composite parent) {
      Composite area = (Composite)super.createDialogArea(parent);
      traceInput = new SharedTraceInput(area, models, widgets, refreshDevices);
      traceInput.setLayoutData(new GridData(SWT.FILL, SWT.FILL, true, false));

      if (devices != null) {
        traceInput.setDevices(models.settings, devices);
      }
      return area;
    }


    @Override
    protected void createButtonsForButtonBar(Composite parent) {
      Button ok = createButton(parent, IDialogConstants.OK_ID, IDialogConstants.OK_LABEL, true);
      createButton(parent, IDialogConstants.CANCEL_ID, IDialogConstants.CANCEL_LABEL, false);

      Listener modifyListener = e -> {
        ok.setEnabled(traceInput.isReady());
      };
      traceInput.addModifyListener(modifyListener);

      modifyListener.handleEvent(null); // Set initial state of widgets.
    }

    @Override
    protected void buttonPressed(int buttonId) {
      if (buttonId == IDialogConstants.OK_ID) {
        value = traceInput.getTraceRequest(models.settings);
      }
      super.buttonPressed(buttonId);
    }

    public void setDevices(List<DeviceCaptureInfo> devices) {
      this.devices = devices;
      traceInput.setDevices(models.settings, devices);
    }


    private static class SharedTraceInput extends Composite {
      private static final String DEFAULT_TRACE_FILE = "trace";
      private static final String TRACE_EXTENSION = ".gfxtrace";
      private static final DateFormat TRACE_DATE_FORMAT = new SimpleDateFormat("_yyyyMMdd_HHmm");
      protected static final String MEC_LABEL = "Trace From Beginning";

      private final String date = TRACE_DATE_FORMAT.format(new Date());
      private final Runnable refreshDevices;
      private LoadingIndicator.Widget deviceLoader;
      private ComboViewer device;
      private List<DeviceCaptureInfo> devices;
      private Label pcsWarning;

      private ActionTextbox uri;
      private ComboViewer api;
      private Text        additionalArgs;
      private Text        cwd;
      private Text        additionalEnvVars;
      private Spinner     frameCount;
      private Button      fromBeginning;
      private Button      withoutBuffering;
      private Button      clearCache;
      private Button      disablePcs;

      protected final FileTextbox.Directory directory;
      protected final Text file;

      protected boolean userHasChangedOutputFile = false;

      public SharedTraceInput(Composite parent, Models models, Widgets widgets,
          Runnable refreshDevices) {
        super(parent, SWT.NONE);
        this.refreshDevices = refreshDevices;
        setLayout(new GridLayout(2, false));

        createLabel(this, "Device:");
        Composite deviceComposite =
            createComposite(this, withMargin(new GridLayout(2, false), 0, 0));
        device = createDeviceDropDown(deviceComposite);
        deviceLoader = widgets.loading.createWidgetWithRefresh(deviceComposite);
        device.getCombo().setLayoutData(new GridData(SWT.FILL, SWT.FILL, true, false));
        deviceLoader.setLayoutData(
            withIndents(new GridData(SWT.RIGHT, SWT.CENTER, false, false), 5, 0));
        // TODO: Make this a true button to allow keyboard use.
        deviceLoader.addListener(SWT.MouseDown, e -> {
          deviceLoader.startLoading();
          // By waiting a tiny bit, the icon will change to the loading indicator, giving the user
          // feedback that something is happening, in case the refresh is really quick.
          Scheduler.EXECUTOR.schedule(refreshDevices, 300, TimeUnit.MILLISECONDS);
        });

        deviceComposite.setLayoutData(new GridData(SWT.FILL, SWT.FILL, true, false));

        createLabel(this, "URI:");
        uri = withLayoutData(new ActionTextbox(this, "") {
          @Override
          protected String createAndShowDialog(String current) {
            if (device.getCombo().getSelectionIndex() >= 0) {
              DeviceCaptureInfo selectedDevice = devices.get(device.getCombo().getSelectionIndex());
              TraceTargetPickerDialog dialog = new TraceTargetPickerDialog(
                  getShell(), models, selectedDevice.targets, widgets);
              if (dialog.open() == Window.OK) {
                TraceTargets.Node node = dialog.getSelected();
                if (node == null) {
                  return null;
                }
                TraceTargetTreeNode data = node.getData();
                return (data == null) ? null : data.getTraceUri();
              }
            }
            return null;
          }
        }, new GridData(SWT.FILL, SWT.FILL, true, false));

        createLabel(this, "API:");
        api = createApiDropDown(this);
        api.getCombo().setLayoutData(new GridData(SWT.FILL, SWT.FILL, true, false));

        createLabel(this, "Additional Arguments:");
        additionalArgs = withLayoutData(
          createTextbox(this, ""), new GridData(SWT.FILL, SWT.FILL, true, false));

        createLabel(this, "Working Directory:");
        cwd = withLayoutData(
          createTextbox(this, ""), new GridData(SWT.FILL, SWT.FILL, true, false));
        cwd.setEnabled(false);

        createLabel(this, "Additional Environment Variables:");
        additionalEnvVars = withLayoutData(
          createTextbox(this, ""), new GridData(SWT.FILL, SWT.FILL, true, false));
        additionalEnvVars.setEnabled(false);

        createLabel(this, "Stop After:");
        Composite frameCountComposite =
            createComposite(this, withMargin(new GridLayout(2, false), 0, 0));
        frameCount = withLayoutData(
            createSpinner(frameCountComposite, models.settings.traceFrameCount, 0, 999999),
            new GridData(SWT.LEFT, SWT.FILL, false, false));
        createLabel(frameCountComposite, "Frames (0 for unlimited)");

        createLabel(this, "");
        fromBeginning = withLayoutData(
            createCheckbox(this, MEC_LABEL, !models.settings.traceMidExecution),
            new GridData(SWT.FILL, SWT.FILL, true, false));
        fromBeginning.setEnabled(false);

        createLabel(this, "");
        withoutBuffering = withLayoutData(
            createCheckbox(this, "Disable Buffering", models.settings.traceWithoutBuffering),
            new GridData(SWT.FILL, SWT.FILL, true, false));

        createLabel(this, "");
        clearCache = withLayoutData(
            createCheckbox(this, "Clear package cache", models.settings.traceClearCache),
            new GridData(SWT.FILL, SWT.FILL, true, false));
        clearCache.setEnabled(false);

        createLabel(this, "");
        disablePcs = withLayoutData(
            createCheckbox(this, "Disable pre-compiled shaders", models.settings.traceDisablePcs),
            new GridData(SWT.FILL, SWT.FILL, true, false));
        disablePcs.setEnabled(false);

        createLabel(this, "Output Directory:");
        directory = withLayoutData(new FileTextbox.Directory(this, models.settings.traceOutDir) {
          @Override
          protected void configureDialog(DirectoryDialog dialog) {
            dialog.setText(Messages.CAPTURE_DIRECTORY);
          }
        }, new GridData(SWT.FILL, SWT.FILL, true, false));

        createLabel(this, "File Name:");
        file = withLayoutData(
            createTextbox(this, ""), new GridData(SWT.FILL, SWT.FILL, true, false));

        file.addListener(SWT.Modify, e -> {
          userHasChangedOutputFile = true;
        });

        createLabel(this, "");
        pcsWarning = withLayoutData(
            createLabel(this, "Warning: Pre-compiled shaders are not supported in the replay."),
            new GridData(SWT.FILL, SWT.FILL, true, false));
        pcsWarning.setForeground(getDisplay().getSystemColor(SWT.COLOR_DARK_YELLOW));
        pcsWarning.setVisible(!models.settings.traceDisablePcs);

        device.getCombo().addListener(SWT.Selection,
          e -> {
            if (device.getCombo().getSelectionIndex() >= 0) {
              DeviceCaptureInfo selectedDevice = devices.get(device.getCombo().getSelectionIndex());
              DeviceTraceConfiguration config = selectedDevice.config;

              cwd.setEnabled(config.getCanSpecifyCwd());
              additionalEnvVars.setEnabled(config.getCanSpecifyEnv());
              clearCache.setEnabled(config.getHasCache());
              updateAPIDropdown(config.getApisList(), models.settings);
            } else {
              cwd.setEnabled(false);
              additionalEnvVars.setEnabled(false);
              clearCache.setEnabled(false);
              updateAPIDropdown(null, models.settings);
            }
        });

        api.getCombo().addListener(SWT.Selection,
          e -> {
            if (api.getCombo().getSelectionIndex() >= 0) {
              DeviceAPITraceConfiguration selectedAPI =
                (DeviceAPITraceConfiguration)(((StructuredSelection)api.getSelection()).getFirstElement());
              if (selectedAPI.getCanDisablePcs()) {
                disablePcs.setEnabled(true);
                disablePcs.setSelection(models.settings.traceDisablePcs);
              } else {
                disablePcs.setEnabled(false);
                disablePcs.setSelection(false);
              }

              if (selectedAPI.getSupportsMidExecutionCapture()) {
                fromBeginning.setEnabled(true);
                fromBeginning.setSelection(!models.settings.traceMidExecution);
              } else {
                fromBeginning.setEnabled(false);
                fromBeginning.setSelection(true);
              }
            } else {
              fromBeginning.setEnabled(false);
              fromBeginning.setSelection(true);
              disablePcs.setEnabled(false);
              disablePcs.setSelection(false);
            }
        });

        updateDevicesDropDown(models.settings);
      }

      private static ComboViewer createDeviceDropDown(Composite parent) {
        ComboViewer combo = createDropDownViewer(parent);
        combo.setContentProvider(ArrayContentProvider.getInstance());
        combo.setLabelProvider(new LabelProvider() {
          @Override
          public String getText(Object element) {
            Device.Instance info = ((DeviceCaptureInfo)element).device;
            StringBuilder sb = new StringBuilder();
            if (!info.getName().isEmpty()) {
              sb.append(info.getName()).append(" - ");
            }
            if (!info.getConfiguration().getOS().getName().isEmpty()) {
              sb.append(info.getConfiguration().getOS().getName()).append(" - ");
            }
            return sb.append(info.getSerial()).toString();
          }
        });
        return combo;
      }

      protected String formatTraceName(String name) {
        return (name.isEmpty() ? DEFAULT_TRACE_FILE : name) + date + TRACE_EXTENSION;
      }

      protected String getDefaultApi(Settings settings) {
        return settings.traceApi;
      }


      private static ComboViewer createApiDropDown(Composite parent) {
        ComboViewer combo = createDropDownViewer(parent);
        combo.setContentProvider(ArrayContentProvider.getInstance());
        combo.setLabelProvider(new LabelProvider() {
          @Override
          public String getText(Object element) {
            return ((DeviceAPITraceConfiguration)element).getApi();
          }
        });
        return combo;
      }

      public boolean isReady() {
        return api.getCombo().getSelectionIndex() >= 0 &&
            !file.getText().isEmpty() && !directory.getText().isEmpty();
      }

      public void addModifyListener(Listener listener) {
        api.getCombo().addListener(SWT.Selection, listener);
        file.addListener(SWT.Modify, listener);
        directory.addBoxListener(SWT.Modify, listener);
        device.getCombo().addListener(SWT.Selection, listener);
      }


      public void setDevices(Settings settings, List<DeviceCaptureInfo> devices) {
        this.devices = devices;
        updateDevicesDropDown(settings);
      }

      private void updateDevicesDropDown(Settings settings) {
        if (device != null && devices != null) {
          deviceLoader.stopLoading();
          device.setInput(devices);
          if (!settings.traceDevice.isEmpty()) {
            Optional<DeviceCaptureInfo> deflt = devices.stream()
                .filter(dev -> settings.traceDevice.equals(dev.device.getSerial()))
                .findAny();
            if (deflt.isPresent()) {
              device.setSelection(new StructuredSelection(deflt.get()));
            }
          }
          device.getCombo().notifyListeners(SWT.Selection, new Event());
        } else if (deviceLoader != null) {
          deviceLoader.startLoading();
        }
      }

      private void updateAPIDropdown(List<DeviceAPITraceConfiguration> configs, Settings settings) {
        api.setInput(configs);
        if (configs != null) {
          for (DeviceAPITraceConfiguration c : configs) {
            if (c.getApi().equals(settings.traceApi)) {
              api.setSelection(new StructuredSelection(c));
              api.getCombo().notifyListeners(SWT.Selection, new Event());
              return;
            }
          }
          api.setSelection(new StructuredSelection(configs.get(0)));
        }
        api.getCombo().notifyListeners(SWT.Selection, new Event());
      }

      public TraceRequest getTraceRequest(Settings settings) {
        settings.traceApi = getSelectedApi();
        settings.traceOutDir = directory.getText();
        settings.traceFrameCount = frameCount.getSelection();
        settings.traceMidExecution = !fromBeginning.getSelection();
        settings.traceWithoutBuffering = withoutBuffering.getSelection();
        DeviceCaptureInfo selectedDevice = devices.get(device.getCombo().getSelectionIndex());
        return new TraceRequest(selectedDevice, uri.getText(),
          getSelectedApi(), getOutputFile(),
          frameCount.getSelection(), !fromBeginning.getSelection(),
          withoutBuffering.getSelection());
      }

      protected String getSelectedApi() {
        return ((DeviceAPITraceConfiguration)api.getStructuredSelection().getFirstElement()).getApi();
      }

      private File getOutputFile() {
        String name = file.getText();
        if (name.isEmpty()) {
          name = formatTraceName(DEFAULT_TRACE_FILE);
        }
        String dir = directory.getText();
        return dir.isEmpty() ? new File(name) : new File(dir, name);
      }
    }
  }

  /**
   * Dialog that shows trace progress to the user and allows the user to stop the capture.
   */
  private static class TraceProgressDialog extends DialogBase {
    private final StringBuilder log = new StringBuilder();
    private final Tracer.TraceRequest request;
    private Text text;
    private Runnable onStart;

    public TraceProgressDialog(Shell shell, Tracer.TraceRequest request, Theme theme) {
      super(shell, theme);
      this.request = request;
    }

    public void setOnStart(Runnable onStart) {
      this.onStart = onStart;
    }

    public void append(String line) {
      ifNotDisposed(text, () -> {
        log.append(line).append(text.getLineDelimiter());
        int selection = text.getCharCount();
        text.setText(log.toString());
        text.setSelection(selection);
      });
    }

    @Override
    public String getTitle() {
      return Messages.CAPTURING_TRACE;
    }

    @Override
    protected Control createDialogArea(Composite parent) {
      Composite area = (Composite)super.createDialogArea(parent);

      Composite container = createComposite(area, new GridLayout(1, false));
      container.setLayoutData(new GridData(SWT.FILL, SWT.FILL, true, true));

      createBoldLabel(container, request.getProgressDialogTitle())
          .setLayoutData(new GridData(SWT.FILL, SWT.TOP, true, false));

      text = createTextarea(container, log.toString());
      text.setEditable(false);
      text.setLayoutData(new GridData(SWT.FILL, SWT.FILL, true, true));

      return area;
    }

    @Override
    protected void createButtonsForButtonBar(Composite parent) {
      createButton(parent, IDialogConstants.OK_ID, request.midExecution ? "Start" : "Stop", true);
    }

    @Override
    protected void buttonPressed(int buttonId) {
      if (IDialogConstants.OK_ID == buttonId && "Start".equals(getButton(buttonId).getText())) {
        getButton(buttonId).setText("Stop");
        onStart.run();
      } else {
        super.buttonPressed(buttonId);
      }
    }
  }
}
