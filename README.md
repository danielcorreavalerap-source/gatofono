# Gatófono (VST3)

Sintetizador de maullidos para Reaper (y cualquier DAW con VST3). Polifónico (8 voces), con escalas, arpegiador, reverb y carga de tu propio audio.

## Compilarlo sin instalar nada (GitHub Actions)
1. Crea un repositorio en GitHub y sube esta carpeta entera (incluida `.github`).
2. Ve a la pestaña **Actions** → **Compilar Gatofono** → espera a que termine (unos 10 min).
3. Descarga el artefacto `Gatofono-VST3-windows-latest` o `Gatofono-VST3-macos-latest`.

## Compilarlo en local
```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target Gatofono_VST3
```
Necesitas CMake 3.22+, y Visual Studio 2022 (Windows) o Xcode (Mac).

## Instalar en Reaper
- Windows: copia `Gatofono.vst3` a `C:\Program Files\Common Files\VST3`
- Mac: copia a `~/Library/Audio/Plug-Ins/VST3` y ejecuta `xattr -cr ~/Library/Audio/Plug-Ins/VST3/Gatofono.vst3`
- Reaper: Preferences → Plug-ins → VST → **Re-scan**. Añade una pista y pon *VSTi: Gatofono*.
