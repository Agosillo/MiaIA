# Project Genesis

> *"Understand AI, don't just run it."*

---

# Why MiaIA Exists

Most Artificial Intelligence frameworks are designed to **build** and **train** neural networks.

Very few tools are designed to **understand** them.

MiaIA was born from a simple question:

> *What if we could explore a neural network the same way a developer explores a program inside an IDE?*

The objective of MiaIA is not to replace existing AI frameworks.

The objective is to make Artificial Intelligence **visible**, **interactive** and **understandable**.

---

# Vision

MiaIA is an Interactive Development Environment (IDE) for Artificial Intelligence.

It allows developers, students and researchers to:

- visualize neural networks;
- inspect every layer;
- inspect every neuron;
- observe forward propagation;
- inspect parameters;
- experiment with models;
- understand how AI actually works.

The project focuses on **understanding** before **optimization**.

---

# Philosophy

Several principles guide every architectural decision.

## Clarity over complexity

A simple architecture is preferred over a clever one.

If something cannot be easily explained, it should probably be redesigned.

---

## One responsibility

Every module has exactly one responsibility.

Engine computes.

Core describes.

Editor visualizes.

Console experiments.

Tests validate.

---

## Human first

The code is written for humans before machines.

Readable code is considered a feature.

---

## Performance comes later

The first implementation must be understandable.

Optimizations belong inside the Engine and must never reduce the readability of the Core.

---

# Architecture

The project is divided into independent modules.

```
MiaIA

├── Engine
├── Core
├── Editor
├── Console
├── Tests
└── Docs
```

## Engine

Low level numerical engine.

Responsibilities:

- vectors
- matrices
- tensors
- optimizers
- numerical algorithms

The Engine knows nothing about neurons.

---

## Core

The logical model.

Responsibilities:

- models
- layers
- neurons
- datasets
- training
- AI abstractions

The Core describes AI.

---

## Editor

The graphical IDE.

Responsibilities:

- visualization
- debugging
- interaction
- exploration

The Editor is implemented using Unreal Engine.

---

## Console

Developer playground.

Responsibilities:

- experiments
- benchmarks
- quick tests
- command line execution

---

## Tests

Automatic validation of every component.

---

# Naming

The project follows a minimal naming philosophy.

Repository

```
MiaIA
```

Solution

```
MiaIA.sln
```

Projects

```
Engine
Core
Editor
Console
Tests
Docs
```

Namespaces

```cpp
namespace Mia::Core
namespace Mia::Engine
namespace Mia::AI
```

Names should be:

- short;
- explicit;
- atomic.

---

# Official Language

The official language of the project is **English**.

Everything is written in English:

- source code;
- comments;
- documentation;
- commit messages;
- issues;
- wiki.

Translations may be provided later.

---

# Long Term Vision

MiaIA does not aim to become another AI framework.

It aims to become the IDE where AI can be explored, understood and experimented with.

Training models is important.

Understanding models is the real objective.

---

# Status

Project started:

**August 2026**

Initial architecture defined before writing the first line of production code.

The project begins from ideas and architecture rather than implementation.

> *"First understand. Then build."*

---

# Authors

**Agostino Mosillo**

Project ideation, architecture and vision.

Developed together with **OpenAI ChatGPT** as a collaborative design partner from the initial conception of the project.