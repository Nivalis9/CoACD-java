# Third-party notices

This repository vendors CoACD 1.0.14 under `vendor/CoACD` so the JNI library
can be built from a standalone checkout.

- CoACD: tag `1.0.14`, copyright Sarah Weiii and contributors. Its MIT license
  is included at `vendor/CoACD/LICENSE`.
- CDT: commit `ec03b309fd18102ab1da069f2edf3b37be5d1fb3`, the revision pinned by
  CoACD 1.0.14. Its licenses are included under `vendor/CoACD/3rd/cdt`.

CoACD's optional preprocessing build downloads additional dependencies through
its CMake configuration. Their respective licenses apply when those components
are built and distributed.
