/*
 @licstart  The following is the entire license notice for the JavaScript code in this file.

 The MIT License (MIT)

 Copyright (C) 1997-2020 by Dimitri van Heesch

 Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 and associated documentation files (the "Software"), to deal in the Software without restriction,
 including without limitation the rights to use, copy, modify, merge, publish, distribute,
 sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:

 The above copyright notice and this permission notice shall be included in all copies or
 substantial portions of the Software.

 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
 BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

 @licend  The above is the entire license notice for the JavaScript code in this file
*/
var NAVTREE =
[
  [ "Monitoring System", "index.html", [
    [ "System Overview", "index.html#overview", null ],
    [ "Key Features", "index.html#features", null ],
    [ "Architecture Diagram", "index.html#architecture", null ],
    [ "Quick Start", "index.html#quickstart", null ],
    [ "Installation", "index.html#installation", [
      [ "CMake FetchContent (Recommended)", "index.html#install_fetchcontent", null ],
      [ "vcpkg", "index.html#install_vcpkg", null ]
    ] ],
    [ "Module Overview", "index.html#modules", null ],
    [ "Examples", "index.html#examples", null ],
    [ "Learning Resources", "index.html#learning_resources", null ],
    [ "Related Systems", "index.html#related", null ],
    [ "Dynamic Plugin Loading Example", "md_examples_2plugin__example_2README.html", [
      [ "Overview", "md_examples_2plugin__example_2README.html#autotoc_md8", null ],
      [ "Files", "md_examples_2plugin__example_2README.html#autotoc_md9", null ],
      [ "Building", "md_examples_2plugin__example_2README.html#autotoc_md10", [
        [ "Build the Plugin Library", "md_examples_2plugin__example_2README.html#autotoc_md11", null ],
        [ "Build the Example Program", "md_examples_2plugin__example_2README.html#autotoc_md12", null ]
      ] ],
      [ "Running", "md_examples_2plugin__example_2README.html#autotoc_md13", null ],
      [ "Expected Output", "md_examples_2plugin__example_2README.html#autotoc_md14", null ],
      [ "Creating Your Own Plugin", "md_examples_2plugin__example_2README.html#autotoc_md15", [
        [ "Implement the collector_plugin Interface", "md_examples_2plugin__example_2README.html#autotoc_md16", null ],
        [ "Export the Plugin", "md_examples_2plugin__example_2README.html#autotoc_md17", null ],
        [ "Build as Shared Library", "md_examples_2plugin__example_2README.html#autotoc_md18", null ],
        [ "Load and Use", "md_examples_2plugin__example_2README.html#autotoc_md19", null ]
      ] ],
      [ "Plugin API Version Compatibility", "md_examples_2plugin__example_2README.html#autotoc_md20", null ],
      [ "Platform-Specific Notes", "md_examples_2plugin__example_2README.html#autotoc_md21", [
        [ "Linux", "md_examples_2plugin__example_2README.html#autotoc_md22", null ],
        [ "macOS", "md_examples_2plugin__example_2README.html#autotoc_md23", null ],
        [ "Windows", "md_examples_2plugin__example_2README.html#autotoc_md24", null ]
      ] ],
      [ "Error Handling", "md_examples_2plugin__example_2README.html#autotoc_md25", null ],
      [ "Security Considerations", "md_examples_2plugin__example_2README.html#autotoc_md26", null ],
      [ "Performance", "md_examples_2plugin__example_2README.html#autotoc_md27", null ]
    ] ],
    [ "Tutorial: Metrics Collection", "tutorial_metrics.html", [
      [ "Goal", "tutorial_metrics.html#metrics_goal", null ],
      [ "Prerequisites", "tutorial_metrics.html#metrics_prereq", null ],
      [ "Step 1: Use a built-in system collector", "tutorial_metrics.html#metrics_step1", null ],
      [ "Step 2: Define a custom metric", "tutorial_metrics.html#metrics_step2", null ],
      [ "Step 3: Register via the factory", "tutorial_metrics.html#metrics_step3", null ],
      [ "Step 4: Time-series storage", "tutorial_metrics.html#metrics_storage", null ],
      [ "Common Mistakes", "tutorial_metrics.html#metrics_mistakes", null ],
      [ "Next Steps", "tutorial_metrics.html#metrics_next", null ]
    ] ],
    [ "Tutorial: Distributed Tracing", "tutorial_tracing.html", [
      [ "Goal", "tutorial_tracing.html#tracing_goal", null ],
      [ "Prerequisites", "tutorial_tracing.html#tracing_prereq", null ],
      [ "Step 1: Start a root span", "tutorial_tracing.html#tracing_step1", null ],
      [ "Step 2: Create child spans", "tutorial_tracing.html#tracing_step2", null ],
      [ "Step 3: Propagate context across services", "tutorial_tracing.html#tracing_step3", null ],
      [ "Step 4: Export via OTLP", "tutorial_tracing.html#tracing_otlp", null ],
      [ "Common Mistakes", "tutorial_tracing.html#tracing_mistakes", null ],
      [ "Next Steps", "tutorial_tracing.html#tracing_next", null ]
    ] ],
    [ "Tutorial: Alert Pipeline", "tutorial_alerts.html", [
      [ "Goal", "tutorial_alerts.html#alerts_goal", null ],
      [ "Step 1: Define a trigger", "tutorial_alerts.html#alerts_step1", null ],
      [ "Step 2: Attach notifiers", "tutorial_alerts.html#alerts_step2", null ],
      [ "Step 3: Register with the pipeline", "tutorial_alerts.html#alerts_step3", null ],
      [ "Graceful Degradation", "tutorial_alerts.html#alerts_degradation", null ],
      [ "Common Mistakes", "tutorial_alerts.html#alerts_mistakes", null ],
      [ "Next Steps", "tutorial_alerts.html#alerts_next", null ]
    ] ],
    [ "Frequently Asked Questions", "faq.html", [
      [ "Collector Questions", "faq.html#faq_collectors", [
        [ "How do I add a custom collector?", "faq.html#faq_custom_collector", null ],
        [ "How often should collectors run?", "faq.html#faq_collector_interval", null ]
      ] ],
      [ "Tracing Questions", "faq.html#faq_tracing", [
        [ "How do I configure OTLP export?", "faq.html#faq_otlp_config", null ],
        [ "Should I sample traces?", "faq.html#faq_trace_sampling", null ]
      ] ],
      [ "Alert Questions", "faq.html#faq_alerts", [
        [ "Circuit breaker vs alert — when should I use which?", "faq.html#faq_cb_vs_alert", null ],
        [ "How do I prevent flapping alerts?", "faq.html#faq_alert_flapping", null ]
      ] ],
      [ "Integration Questions", "faq.html#faq_integration", [
        [ "How does monitoring_system integrate with logger_system?", "faq.html#faq_logger_integration", null ],
        [ "Bidirectional DI — what is it?", "faq.html#faq_bidirectional_di", null ]
      ] ],
      [ "Performance Questions", "faq.html#faq_perf", [
        [ "What's the overhead of instrumentation?", "faq.html#faq_overhead", null ],
        [ "Is the registry thread-safe?", "faq.html#faq_thread_safety", null ]
      ] ],
      [ "Plugin Questions", "faq.html#faq_plugins", [
        [ "Can I load collectors dynamically?", "faq.html#faq_dynamic_plugins", null ]
      ] ],
      [ "Storage Questions", "faq.html#faq_storage", [
        [ "Which storage backends are supported?", "faq.html#faq_storage_backends", null ]
      ] ]
    ] ],
    [ "Troubleshooting Guide", "troubleshooting.html", [
      [ "Missing Metrics", "troubleshooting.html#ts_missing_metrics", null ],
      [ "OTLP Export Failures", "troubleshooting.html#ts_otlp_failures", null ],
      [ "Alert False Positives", "troubleshooting.html#ts_alert_false_positives", null ],
      [ "Memory Growth", "troubleshooting.html#ts_memory_growth", null ],
      [ "Plugin Loader Failures", "troubleshooting.html#ts_plugin_loading", null ]
    ] ],
    [ "Namespaces", "namespaces.html", [
      [ "Namespace List", "namespaces.html", "namespaces_dup" ],
      [ "Namespace Members", "namespacemembers.html", [
        [ "All", "namespacemembers.html", null ],
        [ "Functions", "namespacemembers_func.html", null ],
        [ "Variables", "namespacemembers_vars.html", null ],
        [ "Typedefs", "namespacemembers_type.html", null ],
        [ "Enumerations", "namespacemembers_enum.html", null ]
      ] ]
    ] ],
    [ "Concepts", "concepts.html", "concepts" ],
    [ "Classes", "annotated.html", [
      [ "Class List", "annotated.html", "annotated_dup" ],
      [ "Class Index", "classes.html", null ],
      [ "Class Hierarchy", "hierarchy.html", "hierarchy" ],
      [ "Class Members", "functions.html", [
        [ "All", "functions.html", "functions_dup" ],
        [ "Functions", "functions_func.html", "functions_func" ],
        [ "Variables", "functions_vars.html", "functions_vars" ],
        [ "Typedefs", "functions_type.html", null ],
        [ "Enumerations", "functions_enum.html", null ]
      ] ]
    ] ],
    [ "Files", "files.html", [
      [ "File List", "files.html", "files_dup" ],
      [ "File Members", "globals.html", [
        [ "All", "globals.html", "globals_dup" ],
        [ "Functions", "globals_func.html", "globals_func" ],
        [ "Variables", "globals_vars.html", null ],
        [ "Typedefs", "globals_type.html", null ],
        [ "Macros", "globals_defs.html", null ]
      ] ]
    ] ],
    [ "Examples", "examples.html", "examples" ]
  ] ]
];

var NAVTREEINDEX =
[
"_2home_2runner_2work_2monitoring_system_2monitoring_system_2include_2kcenon_2monitoring_2alert_2alert_config_8h-example.html",
"classHotPathHelperTest.html#a384dfd6b723285f77c723c807764467c",
"classkcenon_1_1monitoring_1_1adapters_1_1performance__monitor__adapter.html#aa9db706b7a2c01bf8e079e343699f6ac",
"classkcenon_1_1monitoring_1_1alert__manager.html#a615b1b3dfdab295bef15cd4b3e48bf5f",
"classkcenon_1_1monitoring_1_1anomaly__trigger.html#a6de1b2fc87731507d4a9724f014c4f72",
"classkcenon_1_1monitoring_1_1collector__base.html#a110a94099963b7782eb6814343a8c7ca",
"classkcenon_1_1monitoring_1_1config__parser.html#a3ab2985a3a656cc427e50bd8e049ac6c",
"classkcenon_1_1monitoring_1_1data__consistency__manager.html#a160512d243d7a31da398137b8336581e",
"classkcenon_1_1monitoring_1_1error__boundary.html#a23dcdf654ac183f16ef4010b4950f7ee",
"classkcenon_1_1monitoring_1_1file__notifier.html#acddf3a550215ca5dd81d24a52bd6ff57",
"classkcenon_1_1monitoring_1_1health__check__event.html#a218a12684ba7a0e24c26d9ca7a5cb50b",
"classkcenon_1_1monitoring_1_1interface__observable.html",
"classkcenon_1_1monitoring_1_1lockfree__queue.html#a54f470cef67bcba94cbbdcdb8b39c128",
"classkcenon_1_1monitoring_1_1metric__exporter__interface.html#ab6d2d6ad834c6f483bfa6b1a5eb05956",
"classkcenon_1_1monitoring_1_1moving__window__aggregator.html#a1e0886738340b1251714a401d8b31102",
"classkcenon_1_1monitoring_1_1otlp__grpc__exporter.html#a1478503f11330869b9741a23a9feeec5",
"classkcenon_1_1monitoring_1_1performance__monitor.html#afc715ec76515c7c53a27aec5e8e13197",
"classkcenon_1_1monitoring_1_1plugin__loader.html#a7e5494f3c460184cff0854f7fedd3c85",
"classkcenon_1_1monitoring_1_1power__collector.html#ac6e3fb0ef17d703e1b8cd1c46fbc386a",
"classkcenon_1_1monitoring_1_1rate__of__change__trigger.html#afcf50f09173dd268d079833a2482925b",
"classkcenon_1_1monitoring_1_1safe__event__dispatcher.html#a03352b804e9ab2cac24f5b92882caa84",
"classkcenon_1_1monitoring_1_1smart__collector.html#a118476eaa10311b154883ce144bca8e4",
"classkcenon_1_1monitoring_1_1statsd__exporter.html#a0b68a1ecb0e86e96eddc9ee09b2c2d3e",
"classkcenon_1_1monitoring_1_1system__info__collector.html#a46625c1624bd1ce3950813899efec2a8",
"classkcenon_1_1monitoring_1_1thread__local__buffer.html",
"classkcenon_1_1monitoring_1_1trace__exporter__interface.html#ad45bb6d7b84726250401db90407b097b",
"classkcenon_1_1monitoring_1_1webhook__notifier.html#a4084adc012cf4549e04128e957b1b8cf",
"collector__plugin_8h.html#aa6d12e10ff6e73808cbf13a50877260aa3ca14c518d1bf901acc339e7c9cd6d7f",
"faq.html#faq_dynamic_plugins",
"md_examples_2plugin__example_2README.html#autotoc_md15",
"namespacekcenon_1_1monitoring.html#a646130481f6be4ea221dedce5c089419a8dc261eb2183f8061f8f7afd6a451be1",
"namespacekcenon_1_1monitoring.html#ae7927fdb839278142ee031c30e42cc98a46f461446cf9afc5a1a32b1f4687cfe6",
"ring__buffer_8h.html#a7c338ab916d1b993ebde2a5e6a62722f",
"structkcenon_1_1monitoring_1_1alert__aggregator__config.html#ae711e418a11473e0396f59c2a8a48c2f",
"structkcenon_1_1monitoring_1_1compact__metric__value.html#adfd3baf25de31a16ce4693a91e4685b7",
"structkcenon_1_1monitoring_1_1event__handler__wrapper.html",
"structkcenon_1_1monitoring_1_1health__monitor__stats.html#ad9a9caa7984f36a0cb66b97865bae6f1",
"structkcenon_1_1monitoring_1_1metric.html#a2bfdfab2f3ec79209fc8298c30620cdf",
"structkcenon_1_1monitoring_1_1network__metrics.html#ae508b7c9157db1d186ee9a591d51c448",
"structkcenon_1_1monitoring_1_1performance__profile.html#a97af429d4e5c3b05415e3a30dcba2bbd",
"structkcenon_1_1monitoring_1_1platform__uptime.html",
"structkcenon_1_1monitoring_1_1resource__quota.html#a7a686dcb3caf94a79b85161194980ca4",
"structkcenon_1_1monitoring_1_1service__state.html#a3f713b1922dc8e08a54245d1ee020482",
"structkcenon_1_1monitoring_1_1summary__data.html#a3eb2bcc74df4c33854548e230f5a86e7",
"structkcenon_1_1monitoring_1_1tcp__state__counts.html",
"structkcenon_1_1monitoring_1_1timer__data_1_1snapshot.html#a0bcd11294353bbaeab72a7df1d103363",
"temperature__collector_8h.html#ad5391888a20ee60ea1d9e24da879aa88",
"test__buffering__strategies_8cpp.html#a22a90800c27a9c1c56a132593b76c4ab",
"test__lock__free__collector_8cpp.html#a916eeb68ec1b788bf14077a2e5f57819",
"test__storage__backends_8cpp.html#aa37787c0fd3f66f52b02f0f345803405"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';