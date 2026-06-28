# DeepStreamDesktop

> A modern Qt Desktop application for managing NVIDIA DeepStream AI services.

---

# Project Vision

DeepStreamDesktop is a portfolio project built with **Qt6**, **Modern C++ (C++20)** and **NVIDIA DeepStream**.

The goal is to build a clean, maintainable desktop application for AI video analytics that demonstrates:

* Modern C++
* Qt Desktop Development
* NVIDIA DeepStream
* gRPC Communication
* Clean Software Architecture
* Real-time Video Processing

This project focuses on software engineering rather than AI model development.

---

# System Architecture

The system consists of two applications.

```text
+---------------------------+
| DeepStreamDesktop         |
| Qt Desktop                |
+------------+--------------+
             |
           gRPC
             |
+------------+--------------+
| DeepStreamService         |
| AI Backend                |
+---------------------------+
```

Desktop is responsible for UI.

DeepStreamService is responsible for AI inference.

All communication goes through gRPC.

---

# Technology Stack

Desktop

* Qt6 Widgets

Backend

* NVIDIA DeepStream
* TensorRT
* OpenCV

Language

* C++20

Communication

* gRPC
* Protocol Buffers

Database

* SQLite

Logging

* spdlog

Build

* CMake

Testing

* GoogleTest

---

# Repository Structure

```text
DeepStreamDesktop/

desktop/

service/

common/

configs/

tests/

CLAUDE.md
```

Keep the repository structure simple.

---

# Core Features

## Camera

* Add Camera
* Edit Camera
* Delete Camera
* Enable / Disable
* Test RTSP Connection

---

## Live View

* Multi Camera
* RTSP Streaming
* Bounding Boxes
* Labels
* Confidence
* FPS

---

## Alerts

* Alert History
* Search
* Filter
* Snapshot

---

## Notifications

* Email
* Slack
* Telegram
* Webhook

---

## Dashboard

* GPU Usage
* CPU Usage
* Memory Usage
* FPS
* Camera Status
* Pipeline Status

---

# Backend Modules

## CameraManager

Responsible for

* Camera CRUD
* Camera Configuration
* Start Camera
* Stop Camera

---

## PipelineManager

Responsible for

* Build DeepStream Pipeline
* Manage RTSP Sources
* Pipeline Lifecycle

---

## DetectionProcessor

Responsible for

* Convert DeepStream metadata
* Filter detections
* Normalize detection data

---

## AlertManager

Responsible for

* Generate alerts
* Save alerts
* Notify Desktop
* Trigger notifications

---

## NotificationManager

Responsible for

* Email
* Slack
* Telegram
* Webhook

---

# Desktop Modules

## CameraPage

Manage cameras.

---

## LiveViewPage

Display live video and bounding boxes.

---

## AlertPage

Display alert history.

---

## DashboardPage

Display system status.

---

## SettingsPage

Manage application settings.

---

# Data Flow

```text
RTSP Camera

↓

PipelineManager

↓

DetectionProcessor

↓

AlertManager

├────────► NotificationManager

└────────► gRPC

↓

Qt Desktop
```

Never bypass this flow.

---

# UI Development Rule

Build UI together with each feature.

Example

* Camera Module → Camera Page
* Live View Module → Live View Page
* Alert Module → Alert Page
* Dashboard Module → Dashboard Page

Do not postpone all UI work until the end.

---

# Coding Rules

Always

* Use C++20
* Use RAII
* Use smart pointers
* Keep classes focused
* Keep functions small
* Prefer composition over inheritance

Never

* Use raw new/delete
* Put business logic into Qt Widgets
* Let UI access DeepStream directly
* Duplicate code

---

# Development Roadmap

## Phase 1 — Foundation

* [ ] Create repository
* [ ] Configure CMake
* [ ] Configure Qt6
* [ ] Configure DeepStream SDK
* [ ] Configure gRPC
* [ ] Configure SQLite
* [ ] Configure spdlog
* [ ] Configure GoogleTest

---

## Phase 2 — Qt Desktop Skeleton

* [ ] MainWindow
* [ ] Sidebar
* [ ] Toolbar
* [ ] Status Bar
* [ ] Dashboard Page
* [ ] Camera Page
* [ ] Live View Page
* [ ] Alert Page
* [ ] Settings Page
* [ ] Navigation

---

## Phase 3 — DeepStreamService Skeleton

* [ ] Create DeepStreamService
* [ ] gRPC Server
* [ ] gRPC Client
* [ ] Health Check API
* [ ] Verify Desktop ↔ Service communication

---

## Phase 4 — Camera Module

* [ ] CameraManager
* [ ] Camera CRUD
* [ ] SQLite
* [ ] Camera Page UI

---

## Phase 5 — DeepStream Pipeline

* [ ] PipelineManager
* [ ] RTSP Input
* [ ] Object Detection
* [ ] Tracking
* [ ] DetectionProcessor

---

## Phase 6 — Live View

* [ ] Video Streaming
* [ ] Detection Streaming
* [ ] Bounding Boxes
* [ ] Labels
* [ ] FPS
* [ ] Live View UI

---

## Phase 7 — Alert Module

* [ ] AlertManager
* [ ] Alert History
* [ ] Search
* [ ] Filter
* [ ] Snapshot
* [ ] Alert Page UI

---

## Phase 8 — Notification Module

* [ ] Email
* [ ] Slack
* [ ] Telegram
* [ ] Webhook

---

## Phase 9 — Dashboard

* [ ] GPU Usage
* [ ] CPU Usage
* [ ] Memory Usage
* [ ] Camera Status
* [ ] Pipeline Status
* [ ] Dashboard UI

---

## Phase 10 — Polish

* [ ] Refactor
* [ ] Unit Tests
* [ ] Improve UI
* [ ] README
* [ ] Screenshots
* [ ] Demo Video

---

## Phase 11 — Deployment

* [ ] Dockerize DeepStreamService
* [ ] Docker Compose
* [ ] Deployment Guide

Desktop can run natively.

DeepStreamService can run inside Docker.

---

# Instructions for Claude Code

Before writing code

1. Read this file completely.
2. Implement only the next unfinished task.
3. Keep Desktop and Service independent.
4. All Desktop ↔ Service communication must use gRPC.
5. UI must never call DeepStream directly.
6. Keep the architecture simple.
7. Do not over-engineer the solution.
8. Reuse existing code whenever possible.
9. If a better design is required, explain the trade-offs before changing the architecture.

The objective is to build a clean, maintainable Qt + DeepStream application suitable for demonstrating Senior C++ Software Engineering skills.
