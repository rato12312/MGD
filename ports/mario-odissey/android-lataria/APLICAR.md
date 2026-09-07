# Aplicar a lataria no APK (codespace, checkout ~/eden)

```bash
ED=~/eden/src/android/app/src/main
cp ports/mario-odissey/android-lataria/res/layout/activity_launcher.xml $ED/res/layout/
cp ports/mario-odissey/android-lataria/res/values/strings.xml $ED/res/values/mgd_strings.xml
```

Depois, no `build.gradle.kts` (app):
- `applicationId = "mgd.port.odyssey"` (namespace org.yuzu MANTÉM — JNI quebra se trocar)

Rebuilda o flavor `GenshinSpoofDebug` (é o "Eden Optimized") e instala.
Interface padrão Android, sem decoração: título, lista, Jogar, Ajustes.
