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
package com.google.gapid.server;

import com.google.common.collect.Lists;
import com.google.common.util.concurrent.FutureCallback;
import com.google.common.util.concurrent.Futures;
import com.google.gapid.models.Settings;
import com.google.gapid.proto.device.Device;
import com.google.gapid.proto.service.Service;
import com.google.gapid.models.Devices.DeviceCaptureInfo;

import org.eclipse.swt.widgets.Display;

import java.io.File;
import java.util.concurrent.CountDownLatch;
import java.util.List;
import java.util.Timer;
import java.util.TimerTask;
import java.util.logging.Logger;

/**
 * Handles capturing an API trace.
 */
public class Tracer {
  private static final Logger LOG = Logger.getLogger(Tracer.class.getName());

  public static Trace trace(
      Client client,
      Display display, Settings settings, TraceRequest request, Listener listener) {
    
    CountDownLatch signal = new CountDownLatch(1);

    StreamSender<Service.TraceRequest> sender = client.streamTrace(
      message -> {
          if (message.getStatus().getStatus() == Service.TraceStatus.Done) {
            signal.countDown();
          }
          display.asyncExec(() -> {
            listener.onProgress(message.toString());
          });
      }
    );

    Timer timer = new Timer();
    

    Futures.addCallback(sender.closed(), new FutureCallback<Void>() {
      @Override
      public void onFailure(Throwable t) {
        timer.cancel();
        // Give some time for all the output to pump through.
        display.asyncExec(() -> display.timerExec(500, () -> listener.onFailure(t)));
        signal.countDown();
      }

      @Override
      public void onSuccess(Void v) {
        timer.cancel();
        signal.countDown();
        // Ignore.
      }
    });

    sender.send(
      Service.TraceRequest.newBuilder()
        .setInitialize(
          Service.TraceOptions.newBuilder()
            .setDevice(request.device.path)
            .setUri(request.uri)
            .setDeferStart(request.midExecution)
            .addApis(request.api)
            .setServerLocalSavePath(request.output.getAbsolutePath())
            .build()
        ).build());

    timer.scheduleAtFixedRate(new TimerTask() {
      @Override 
      public void run() {
        sender.send(
          Service.TraceRequest.newBuilder()
          .setQueryEvent(Service.TraceEvent.Status)
          .build());
      }
    }, 2000, 2000);

    return new Trace() {
      @Override
      public void start() {
        sender.send(
          Service.TraceRequest.newBuilder()
          .setQueryEvent(Service.TraceEvent.Begin)
          .build());
      }

      @Override
      public void stop() {
        sender.send(
          Service.TraceRequest.newBuilder()
          .setQueryEvent(Service.TraceEvent.Stop)
          .build());
        try {
          signal.await();
        } catch(InterruptedException ex) {}
          sender.finish();
      }
    };
  }

  @SuppressWarnings("unused")
  public static interface Listener {
    /**
     * Event indicating output from the tracing process.
     */
    public default void onProgress(String message) { /* empty */ }

    /**
     * Event indicating that tracing has failed.
     */
    public default void onFailure(Throwable error) { /* empty */ }
  }

  /**
   * Trace callback interface.
   */
  public static interface Trace {
    /**
     * Requests the current trace to start capturing. Only valid for mid-execution traces.
     */
    public void start();

    /**
     * Requests the current trace to be stopped.
     */
    public void stop();
  }

  /**
   * Contains information about how and what application to trace.
   */
  public static class TraceRequest {
    public final String api;
    public final File output;
    public final int frameCount;
    public final boolean midExecution;
    public final boolean disableBuffering;
    public final String uri;
    public final DeviceCaptureInfo device;

    public TraceRequest(
        DeviceCaptureInfo device,
        String uri,
        String api, 
        File output, int frameCount, boolean midExecution, boolean disableBuffering) {
      this.device = device;
      this.uri = uri;
      this.api = api;
      this.output = output;
      this.frameCount = frameCount;
      this.midExecution = midExecution;
      this.disableBuffering = disableBuffering;
    }

    public String getProgressDialogTitle() {
      // DO NOT CHECK THIS IN, JUST GETTING THINGS WORKING
      return "foo";
    }
  }
}
