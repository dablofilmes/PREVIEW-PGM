# Preview PGM Plugin

_Read this in [Português](README.pt-br.md) or [English](README.md)._

**Preview PGM** is a plugin developed for OBS Studio that creates a dedicated, floating window for real-time monitoring of the Program (PGM) output, combined with the volume meter and control of a specific audio source chosen by the user.

Additionally, the plugin allows you to define which monitor the window will be displayed on in full screen, making it an excellent solution for creating independent reference monitors for the audio operator or the technical director.

## 🚀 Key Features

* **Dedicated PGM Window:** Real-time visualization of the final broadcast output (PGM).
* **Configurable Full Screen Display:** Choose exactly which monitor the PGM window will be projected on in full screen mode.
* **Integrated Audio Mixer:** Displays the audio level meter (VU meter) of the selected audio source directly on the monitor interface.
* **Custom Source Selection:** Choose which specific audio channel or source you want to monitor side-by-side with the video.
* **Ideal for Reference Monitors:** Perfect for sending to a second or third monitor focused on the live stream's sound operator.

## 🖥 Compatibility

| Operating System | Supported | Notes |
| :--- | :---: | :--- |
| **Windows 10/11** (64-bit) | ✅ | Exclusive support at the moment |

## 📦 How to Install (Windows)

1. Go to the **Releases** section and download the latest version in `.zip` format.
2. Extract the contents of the `.zip` file.
3. Copy the extracted folders (`obs-plugins` and `data`) and paste them into the root folder of your OBS Studio:
   * Default path: `C:\Program Files\obs-studio\`
4. If prompted by Windows, confirm to merge the folders.
5. Restart OBS Studio.

## 💡 How to Use

1. Open OBS Studio.
2. In the top menu, go to **Tools** and click on **Abrir PREVIEW PGM** (Open PREVIEW PGM).
3. In the window settings:
   * Choose which audio source you want to track on the integrated mixer.
   * Select which monitor the window should be displayed on when entering Full Screen mode.
4. Position or activate full screen on the chosen reference monitor. To exit Full Screen mode, press `ESC`.

## 🛠 Contributing

Contributions are always welcome! Feel free to open issues for bug reports or feature suggestions, as well as submit pull requests.

## 📄 License

This project is licensed under the GPL-2.0 License — see the license file for more details.
