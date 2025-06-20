# FragmentUnreal

**FragmentUnreal** is an Unreal Engine plugin designed to import and visualize BIM models in the [Fragment 2.0](https://github.com/ThatOpen/engine_fragment) format. It bridges architecture and construction data with immersive 3D visualization in Unreal Engine.

> 🔄 This plugin is actively developed. Community feedback and contributions are welcome!

---

## 🚀 Features

- 🏗 Import BIM models in Fragment 2.0 format
- 🧱 Translate model hierarchy, geometry, and metadata into Unreal-native assets

---

## 📦 Installation

### 1. Clone the Plugin

```bash
git clone https://github.com/bastto/FragmentsUnreal.git
````

Or add it as a submodule (recommended for multi-repo projects):

```bash
git submodule add https://github.com/bastto/FragmentsUnreal.git
```

### 2. Enable in Your Project

1. Open your Unreal project
2. Go to **Edit > Plugins**
3. Search for "FragmentUnreal" and enable it
4. Restart the editor

---

## 🛠 Usage - (Blueprints)

1. Prepare a model in Fragment 2.0 format
2. From actor or UI Get the FragmentsImporterSubsystem
3. From the FragmentsImporterSubsystem call the function ProcessFragment
4. The plugin will generate actors, meshes, and materials in-scene
5. Hierarchies, transforms, and metadata will be applied based on the input

---

## 🛠 Usage - (C++)

1. Prepare a model in Fragment 2.0 format
2. Add FragmentsUnreal as dependency in your Module
3. Using the UFragmentsImporterSubsystem class call the ProcessFragment function.
4. The plugin will generate actors, meshes, and materials in-scene
5. Hierarchies, transforms, and metadata will be applied based on the input

---


## 📁 Repository Structure

```
FragmentUnreal/
├── Source/
│   ├── FragmentUnreal/                    # Core plugin code
|   │   ├── Fragment/                      # Fragment class to store element's data
|   │   ├── Importer/                      # Fragments import logic
|   │   ├── Index/                         # flatbuffers Fragment structure reference
|   │   └── Utils/                         # Functions library
├── ThirdParty/                            # ThirdParty dependencies
│   ├── FlatBuffers/include/flatbufferss   # Flatbuffers structure
│   ├── libtess2                           # libtess2 for mesh triangulation
|   │   ├── include/                       # tesselator header file
|   │   ├── Lib/                           # Libraries for Android/ARM64 and Win64
|   │   └── Source/                        # Source files
├── Content/                               # Assets
│   ├── Materials                          # Base Materials
│   └── Resources                          # Fragments file example
├── FragmentUnreal.uplugin                 # Plugin descriptor
```

---

## 🧩 Compatibility

* **Unreal Engine**: > 5.3
* **Platforms**: Windows, Android (runtime)
* **Dependencies**: FlatBuffers (included)
* **Dependencies**: libtess2 (included)

---

## 🤝 Contributing

We welcome pull requests and feature ideas!

### 📌 Contribution Guidelines

* Create a fork or work on a feature branch
* Write clean, commented, and modular code
* Follow Unreal Engine C++ standards
* Add a helpful description to your pull request
* All changes go through `dev` and must be merged via PR

---

## 🛡 License

This project is licensed under the [MIT License](./LICENSE).
You are free to use, modify, and distribute it with attribution.

---

## 🙋‍♂️ Maintainer

**Jairo Basto Picott** [@jairo-picott](https://github.com/jairo-picott).
Msc. Civil Engineer | Software developer
Feel free to open issues or email if you're using this in a real-world application!

---

## 🌍 Join the Community

If you're integrating Fragment BIM data into Unreal, let’s connect and collaborate! Contributions, issues, or even just stars are appreciated. 🌟

---

## 💖 Support the Project

If FragmentUnreal helps you save development time or adds value to your workflow, consider supporting its development.

Your contribution helps cover maintenance, new features, documentation, and plugin support.

Even a small monthly donation goes a long way!
---
