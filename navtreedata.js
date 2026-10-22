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
    [ "Deprecated List", "deprecated.html", null ],
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
"classkcenon_1_1monitoring_1_1moving__window__aggregator.html#a1a24f636ee8b5c1bec730c318ad7a0fa",
"classkcenon_1_1monitoring_1_1otlp__grpc__exporter.html",
"classkcenon_1_1monitoring_1_1performance__monitor.html#ae71bc13cba05ac28365771e83e035ad9",
"classkcenon_1_1monitoring_1_1plugin__loader.html#af220f7a2f5c1e30da0a2da91f6f4d673",
"classkcenon_1_1monitoring_1_1process__metrics__collector.html#ac3b381c6907c68f959bc93768a3c2953",
"classkcenon_1_1monitoring_1_1retry__executor.html#a1845c045fcf28df07b9edff6543a6d20",
"classkcenon_1_1monitoring_1_1scoped__span.html#ac76e90aaed94a6eadf29130aa339eabb",
"classkcenon_1_1monitoring_1_1smart__info__collector.html#a696cb5603f56fb68f4453629f1d71b83",
"classkcenon_1_1monitoring_1_1stream__aggregator.html",
"classkcenon_1_1monitoring_1_1system__resource__collector.html#a00db4973f55ab61277fc1e430c6129f0",
"classkcenon_1_1monitoring_1_1thread__to__monitoring__adapter.html#a6f99fedb0007913c40a5601bb55053a4",
"classkcenon_1_1monitoring_1_1transaction__manager.html#a9e2cab88b09e324736d68dbf57a50d71",
"classmetrics__collector__interface.html#a152adbf65495f3a7de783632da23ae64",
"container__plugin_8h.html#aef868dc9fa5de5034063d48897755986a05b6053c41a2130afd6fc3b158bda4e6",
"functions_func_m.html",
"metric__exporters_8h.html#a02c7867f24a6cbd907fdc07b4f294727a886bb73b3156b0aa24aac99d2de0b238",
"namespacekcenon_1_1monitoring.html#a70e5bf91e06358d079408fe66979cfab",
"namespacekcenon_1_1monitoring.html#afa67b262c914bd24d2eb60839692dfdf",
"protobuf__wire_8h.html#a81173c1dd106536ac03c1d7351e81b26",
"structkcenon_1_1monitoring_1_1alert.html#a2618d457a7d1f54f9b1653c4a4beb7ee",
"structkcenon_1_1monitoring_1_1central__collector_1_1stats.html",
"structkcenon_1_1monitoring_1_1error__info.html#a813320274aefb373f89aafe5ad9a9054",
"structkcenon_1_1monitoring_1_1health__check__result.html#a91388685dd31af13e8e25caa9ad4c995",
"structkcenon_1_1monitoring_1_1load__average__sample.html#ae36deaf93cfca9918cf2644f8f30f722",
"structkcenon_1_1monitoring_1_1metric__storage__stats.html#adaf80f0acf91056502a5aa8e46688e2b",
"structkcenon_1_1monitoring_1_1otel__span__data.html#aaa0d8ff6d231945f396f4a868296bcde",
"structkcenon_1_1monitoring_1_1platform_1_1tcp__state__info.html#a50fcbbee8e8a7da751ec1d39bea87c6a",
"structkcenon_1_1monitoring_1_1process__metrics.html#a080c36c33e80875e22983add994d1b23",
"structkcenon_1_1monitoring_1_1rule__definition_1_1trigger__config.html#a311c85ca9133062f8341df96cb6b2e07",
"structkcenon_1_1monitoring_1_1statsd__metric__data.html#af2ecab91fac8eca66ea221461de69db3",
"structkcenon_1_1monitoring_1_1system__resources_1_1cpu__metrics_1_1load__average.html#a2789d7b916e97e78375e42b98b48298b",
"structkcenon_1_1monitoring_1_1time__series__query.html#a5b1e334733739d34850caeb27e7b2027",
"structkcenon_1_1monitoring_1_1validation__config.html#a07580911f0f710e3191eed21532e6bc6",
"test__alert__manager_8cpp.html#ac50e476c43f562f27dc1955e190fb9b0",
"test__distributed__tracing_8cpp.html#a6fb29c5a64b3185e8a2ef31b046c4f6d",
"test__opentelemetry__adapter_8cpp_source.html",
"test__timer__metrics_8cpp.html#ae083841eef236ae6f706e0ae5749db08"
];

var SYNCONMSG = 'click to disable panel synchronisation';
var SYNCOFFMSG = 'click to enable panel synchronisation';