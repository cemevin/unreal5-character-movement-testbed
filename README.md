# Unreal Engine 5: Advanced Character Movement & Multiplayer Testbed

![Unreal Engine](https://img.shields.io/badge/Unreal_Engine-5.0+-black?logo=unrealengine)
![C++](https://img.shields.io/badge/Language-C++-blue)
![Multiplayer](https://img.shields.io/badge/Networking-Replicated-success)

Welcome to my Unreal Engine 5 testbed project! This repository serves as a showcase of my gameplay engineering and multiplayer programming skills, specifically focusing on advanced, fluid player character controls.

## Video Demonstration

Click the image below to watch a full breakdown of the movement mechanics in action:

[![Watch the video](https://img.youtube.com/vi/G12nkVgx63M/maxresdefault.jpg)](https://youtu.be/G12nkVgx63M)

## Features Implemented

This project includes custom C++ components to handle complex character traversal and abilities, all built with **full multiplayer replication** in mind.

* **Dash Mechanics:** A robust dash component complete with dynamic camera Field of View (FOV) adjustments and speedline visual effects (VFX) for high-impact game feel.
* **Advanced Ledge Climbing:** Utilizes UE5's **Motion Warping** to accurately adjust and place the character's transform during the ledge climb animation, ensuring seamless alignments regardless of the ledge's height or angle.
* **Wall Running:** Smooth, gravity-defying wall running logic.
* **Movement Chaining:** The architecture allows players to fluidly combine these movements (e.g., dashing into a wall run, jumping off into a ledge climb).
* **Networked Multiplayer:** Every mechanic listed above is fully replicated, ensuring smooth gameplay for both listen servers and connected clients without desyncs.

## Technical Details
* **Core Logic:** Primarily driven by **C++** (~95% of the codebase).
* **Animation:** Uses UE5's modern animation features (Motion Warping) to disconnect rigid animations from dynamic in-game geometry.

## Getting Started

To explore the code or run this project locally:

1. Clone this repository:
   ```bash
   git clone [https://github.com/cemevin/unreal5-character-test.git](https://github.com/cemevin/unreal5-character-test.git)
