---
title: "GPU look shader in ClipFrameItem: RHI custom-material gotchas"
author: Nodaysidle
date: 2026-11-24
branch: master
id: l-fe2c963c
scope: repo
status: active
src: agent
touches:
  - src/clipframeitem.cpp
  - tests/cutcap_tests.cpp
---

Custom QSGMaterialShader on the RHI scene graph (ClipFrameItem look pipeline moved CPU -> GPU):
(1) shaders must be .qsb baked with "qsb --qt6 --batchable" and loaded via setShaderFileName - QShader has no fromSource in Qt 6.12;
(2) updateSampledImage MUST call texture->commitTextureOperations(state.rhi(), state.resourceUpdateBatch()) or the sampler binds the renderer's dummy texture and samples black;
(3) subclassing QSGSimpleTextureNode and calling setMaterial is not enough - also setOpaqueMaterial(nullptr), else Qt's own texture material still renders;
(4) custom uniforms live in the std140 block after mat4+float (a vec4 array starts at offset 80); the uniform buffer is sized to the max block over stages;
(5) sampler at binding 1, uniform block at binding 0, vertex inputs at locations 0/1 (qt_Vertex vec4, qt_MultiTexCoord0 vec2);
(6) a LookNode must not be reused as a plain texture node when the look returns to identity - its material keeps the old uniforms, recreate the node on type switch;
(7) grabWindow on the offscreen platform has no alpha buffer (alpha always 255), so compare premultiplied RGB channels only.
Result: look.frag matches CPU ClipFrameItem::process() with max per-channel delta 1; blur radii beyond the shader tap budget (16) fall back to the CPU path.
