# TS3 Voice Client

Minimalist voice client based on TeamSpeak SDK.

> **Server:** Linux | **Client:** Windows

## About

Lightweight voice communication solution built on proven technologies:

- **Win32 API** — native Windows UI without frameworks
- **TeamSpeak SDK** — professional audio quality with low latency
- **Pure C** — minimal dependencies, maximum performance

### Specs

| Parameter | Value |
|-----------|-------|
| RAM | ~4 MB |
| Disk | ~6 MB |
| Dependencies | TeamSpeak SDK only |
| Audio quality | OPUS codec, highest quality |

Runs on any Windows device, including low-end machines and virtual machines.

## Download

You can download the pre-built `.exe` from [Releases](../../releases) or build from source.

## Build from Source

### Project Structure

```
src/
├── client.c
├── .env
├── res/
│   ├── app.ico
│   ├── app.manifest
│   └── resources.rc
└── teamspeak_sdk/
    ├── include/
    └── bin/
```

### Server Setup (Linux)

Deploy the server on a machine with a static IP:

```bash
cd ts_server

# Compile
gcc server.c -I./teamspeak_sdk/include -L./teamspeak_sdk/bin -lts3server -Wl,-rpath,'$ORIGIN/teamspeak_sdk/bin' -o server

# Run
./server
```

### Client Setup (Windows)

Create `.env` file in `src/` folder:

```env
IP=YOUR_SERVER_IP
PORT=9987
PASSWORD=YOUR_SERVER_PASSWORD
```

Compile the client (requires Visual Studio Build Tools):

```cmd
cd src

:: Compile resources
rc res\resources.rc

:: Compile client
cl /O2 /GL /MT /I"teamspeak_sdk\include" client.c res\resources.res /link /LTCG /OPT:REF /OPT:ICF /LIBPATH:"teamspeak_sdk\bin" ts3client.lib user32.lib gdi32.lib /SUBSYSTEM:WINDOWS /OUT:client.exe
```

### Launch

1. Start the server on Linux
2. Run `client.exe` on Windows
3. Client will automatically connect to the server

## Usage

### Create Channel

1. Enter channel name
2. Enter password (required)
3. Click **Create** or press Enter
4. You will automatically join the created channel

### Join Channel

1. Enter channel ID (shown in channel list)
2. Enter channel password
3. Click **Move** or press Enter

### Microphone Control

- **Mute/Unmute** button — toggle microphone
- Voice activation enabled by default