# MuteMotion-SteamVR-Shim
### (alpha version - needs further development)

Official implementation of the **MuteMotion PCVR** shim/layer for SteamVR.
This repository serves as the technical deliverable for academic auditing at **my university**.

## Architecture
This module bridges hardware IMU data (extracted directly through the OpenVR API) with the underlying math engine. It is designed as a platform-specific "Body" to ensure algorithm portability across VR ecosystems.

* **Target:** PCVR (Windows/Linux)
* **Status:** Academic Proof-of-Concept

## Licensing
* **Shim Layer:** No license - code is currently copyrighted to astro@n0t.space 
 *<sub>(for now, but may change, any questions don't hesitate to contact me, i don't bite! )</sub>*
* **Core Logic:** Proprietary / Restricted Research via MuteMotion Tech.

## Author

* **_AstroN0T** - Sole Architect and Lead Developer
  * *Responsibilities:* End-to-end system architecture, C++ overlay daemon (OpenVR integration, rendering pipeline, physics tuning), PySide6 desktop control panel, Flask verification service, infrastructure provisioning, and end-to-end validation.

## Contributions (Academic)

* **Ciago A. (a22405260)**
  * *Responsibilities:* Contributed to documentation, development of diagrams, presentation support, and mockups.
* **Henrique F.**
  * *Responsibilities:* Contributed to documentation, development of diagrams, presentation support, and mockups.

## Contact
* Technical: astro@n0t.space
* Institutional: astro@n0t.space
