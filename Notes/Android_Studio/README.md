## Android Studio - rAudio app

### Setup
- Device Manager > Virtual > Create device
	- Select Hardware > Phone > `Pixel 2`
	- System Image > Release Name > `S` (x86_64) > Download

### Project
- New Project > Phone and Tablet > Empty Activitiy
	- Name: `rAudio`
	- Package Name: `com.raudio`
	- Language: `Java`
	- Maximum SDK: `Android 22`
- Files
	- `rAudio/app/src/main/AndroidManifest.xml`
	- `rAudio/app/src/main/res/layout/activity_main.xml`
	- `rAudio/app/src/main/java/com/raudio/MainActivity.java`
- Icons
	- `rAudio/app` > New > Image Asset
	- Source > 512 x 512 `icon.png`
	- Delete all existing `*.wepp`s
- Install file
	- Build > Generate Signed Bubled / APK > APK
