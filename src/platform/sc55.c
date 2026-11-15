/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "sc55.h"
#include <clap/clap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL3/SDL.h>
#include "hmp.h"
#include "os.h"

/* The plugin file of DOSBox Staging, its folders, and the environment variable that names its ROM folders. */
#define PLUGIN_FILE   "Nuked-SC55.clap"
#define PLUGIN_FOLDER "plugins"
#define ROM_FOLDER    "soundcanvas-roms"
#define ROM_PATH_ENV  "SOUNDCANVAS_ROM_PATH"
#define BUNDLE_BINARY "Contents/MacOS" /* the folder of the library inside a macOS plugin bundle */

#ifdef _WIN32
/* The separator of the folders in ROM_PATH_ENV. */
#define ROM_PATH_SEPARATOR ";"
#else
/* The separator of the folders in ROM_PATH_ENV. */
#define ROM_PATH_SEPARATOR ":"
#endif

/* The native sample rates of the models. */
#define SC55_RATE     32000
#define SC55_MK2_RATE 33103

/* Audio blocks and buffers of the render thread. */
#define BLOCK_FRAMES    64   /* 2 ms at SC55_RATE: a MIDI message waits at most one block */
#define QUEUED_FRAMES   1323 /* 30 ms at MUSIC_SAMPLE_RATE: the render thread keeps this much in the stream */
#define MESSAGE_QUEUE   1024 /* MIDI messages between two blocks */
#define MAX_AUDIO_PORTS 4
#define MAX_CHANNELS    8 /* the channels of all audio ports of one direction */
#define BEND_RANGE_SEND 6 /* the messages that set the pitch bend range */
#define GS_RESET_SIZE   11

/* MIDI controllers of the registered parameter that sets the pitch bend range. */
#define MIDI_CTRL_DATA_ENTRY     0x06
#define MIDI_CTRL_DATA_ENTRY_LSB 0x26
#define MIDI_CTRL_RPN_LSB        0x64
#define MIDI_CTRL_RPN_MSB        0x65
#define RPN_BEND_RANGE           0x00
#define RPN_NONE                 0x7f

/* The bytes of the GS reset system exclusive message of a Roland GS device. */
#define SYSEX_START           0xf0
#define SYSEX_END             0xf7
#define ROLAND_ID             0x41
#define ROLAND_DEVICE_ID      0x10
#define GS_MODEL_ID           0x42
#define ROLAND_DATA_SET       0x12
#define GS_RESET_ADDRESS_HIGH 0x40
#define GS_RESET_ADDRESS_MID  0x00
#define GS_RESET_ADDRESS_LOW  0x7f
#define GS_RESET_VALUE        0x00
#define GS_RESET_CHECKSUM     0x41

/* A folder where DOSBox Staging keeps the plugins or the ROMs: path below the value of the environment variable env
   (below HOME/home_part when env is not set), or path as it is without env. */
typedef struct {
    const char *env;
    const char *home_part;
    const char *path;
} Place;

/* An SC-55 model of the plugin: the text in its plugin name and its sample rate. */
typedef struct {
    const char *name;
    int rate;
} Model;

/* A MIDI channel message on its way from the game to the render thread. */
typedef struct {
    uint8_t bytes[MIDI_EVENT_SIZE];
} Message;

/* The audio ports of one direction of the plugin and their sample buffers. */
typedef struct {
    clap_audio_buffer_t ports[MAX_AUDIO_PORTS];
    float *channels[MAX_CHANNELS];
    float samples[MAX_CHANNELS][BLOCK_FRAMES];
    uint32_t count;
} AudioPorts;

/* The models in the order of DOSBox Staging: the best SC-55 first. */
static const Model models[] = {
    {"Roland SC-55 v1.21",    SC55_RATE    },
    {"Roland SC-55 v1.20",    SC55_RATE    },
    {"Roland SC-55 v1.10",    SC55_RATE    },
    {"Roland SC-55 v1.00",    SC55_RATE    },
    {"Roland SC-55 v2.00",    SC55_RATE    },
    {"Roland SC-55mk2 v1.01", SC55_MK2_RATE},
    {"Roland SC-55mk2 v1.00", SC55_MK2_RATE},
};

/* The GS reset that the driver sends before the first song. */
static const uint8_t gs_reset[GS_RESET_SIZE] = {
    SYSEX_START,
    ROLAND_ID,
    ROLAND_DEVICE_ID,
    GS_MODEL_ID,
    ROLAND_DATA_SET,
    GS_RESET_ADDRESS_HIGH,
    GS_RESET_ADDRESS_MID,
    GS_RESET_ADDRESS_LOW,
    GS_RESET_VALUE,
    GS_RESET_CHECKSUM,
    SYSEX_END,
};

#ifdef _WIN32
/* The folders of the plugin in a DOSBox Staging installation and its configuration folder. */
static const Place plugin_places[] = {
    {"LOCALAPPDATA", NULL, "Programs/DOSBox Staging/" PLUGIN_FOLDER},
    {"ProgramFiles", NULL, "DOSBox Staging/" PLUGIN_FOLDER         },
    {"LOCALAPPDATA", NULL, "DOSBox/" PLUGIN_FOLDER                 },
};
/* The ROM folder in the configuration folder of DOSBox Staging. */
static const Place rom_places[] = {
    {"LOCALAPPDATA", NULL, "DOSBox/" ROM_FOLDER},
};
#elif defined(__APPLE__)
/* The folders of the plugin in a DOSBox Staging installation and its configuration folder. */
static const Place plugin_places[] = {
    {NULL,   NULL, "/Applications/DOSBox Staging.app/Contents/PlugIns"},
    {"HOME", NULL, "Applications/DOSBox Staging.app/Contents/PlugIns" },
    {"HOME", NULL, "Library/Preferences/DOSBox/" PLUGIN_FOLDER        },
};
/* The ROM folder in the configuration folder of DOSBox Staging. */
static const Place rom_places[] = {
    {"HOME", NULL, "Library/Preferences/DOSBox/" ROM_FOLDER},
};
#else
/* The folders of the plugin in a DOSBox Staging installation and its configuration folder. */
static const Place plugin_places[] = {
    {"XDG_DATA_HOME",   ".local/share", "dosbox-staging/" PLUGIN_FOLDER                 },
    {NULL,              NULL,           "/usr/local/share/dosbox-staging/" PLUGIN_FOLDER},
    {NULL,              NULL,           "/usr/share/dosbox-staging/" PLUGIN_FOLDER      },
    {"XDG_CONFIG_HOME", ".config",      "dosbox/" PLUGIN_FOLDER                         },
};
/* The ROM folders in the data and configuration folders of DOSBox Staging. */
static const Place rom_places[] = {
    {"XDG_DATA_HOME",   ".local/share", "dosbox/" ROM_FOLDER  },
    {"XDG_DATA_HOME",   ".local/share", "soundcanvas-rom-data"},
    {"XDG_CONFIG_HOME", ".config",      "dosbox/" ROM_FOLDER  },
};
#endif

/* The plugin, its render thread, the MIDI queue, and the audio stream. */
static struct {
    SDL_SharedObject *library;
    const clap_plugin_entry_t *entry;
    const clap_plugin_t *plugin; /* NULL while the driver is closed */
    SDL_AudioStream *stream;     /* native rate float in, MUSIC_SAMPLE_RATE 16-bit out */
    SDL_Thread *thread;
    SDL_AtomicInt stop;
    SDL_Mutex *lock; /* guards queue and queued */
    Message queue[MESSAGE_QUEUE];
    int queued;
    /* The state of the render thread. */
    clap_event_midi_t events[MESSAGE_QUEUE];
    uint32_t event_count;
    clap_event_midi_sysex_t reset;
    bool reset_due; /* the GS reset goes before the events of the next block */
    AudioPorts inputs, outputs;
    float stereo[BLOCK_FRAMES * MUSIC_CHANNELS];
} sc55;

/* Folders and files */

/* Writes folder/name into path; false when it does not fit. */
static bool join_path(char *path, size_t size, const char *folder, const char *name)
{
    int written = snprintf(path, size, "%s/%s", folder, name);
    return written > 0 && (size_t)written < size;
}

/* Writes the folder of a place into path; false when it has no base folder or does not fit. */
static bool place_folder(const Place *place, char *path, size_t size)
{
    const char *base = place->env ? getenv(place->env) : NULL;
    int written;
    if (!place->env)
        written = snprintf(path, size, "%s", place->path);
    else if (base && *base)
        written = snprintf(path, size, "%s/%s", base, place->path);
    else if (place->home_part && getenv("HOME"))
        written = snprintf(path, size, "%s/%s/%s", getenv("HOME"), place->home_part, place->path);
    else
        return false;
    return written > 0 && (size_t)written < size;
}

static bool is_folder(const char *path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_DIRECTORY;
}

static bool is_file(const char *path)
{
    SDL_PathInfo info;
    return SDL_GetPathInfo(path, &info) && info.type == SDL_PATHTYPE_FILE;
}

/* Adds folder to the list of ROM folders when it exists. */
static void add_rom_folder(const char *folder, char *list, size_t size)
{
    if (!is_folder(folder))
        return;
    size_t used = strlen(list);
    snprintf(list + used, size - used, "%s%s", used ? ROM_PATH_SEPARATOR : "", folder);
}

/* Sets ROM_PATH_ENV to the ROM folders that exist: roms alone when given, or the ROM folder of the game folder and
   of DOSBox Staging. False with a message when none exists. */
static bool set_rom_folders(const char *roms)
{
    char list[OS_PATH_SIZE * (SDL_arraysize(rom_places) + 1)] = "";
    char folder[OS_PATH_SIZE];
    if (roms)
        add_rom_folder(roms, list, sizeof list);
    else {
        add_rom_folder(ROM_FOLDER, list, sizeof list);
        for (size_t i = 0; i < SDL_arraysize(rom_places); i++)
            if (place_folder(&rom_places[i], folder, sizeof folder))
                add_rom_folder(folder, list, sizeof list);
    }
    if (!*list) {
        if (roms)
            os_log("SC-55: no ROM folder %s", roms);
        else if (place_folder(&rom_places[0], folder, sizeof folder))
            os_log("SC-55: no ROM folder %s or %s", ROM_FOLDER, folder);
        return false;
    }
    os_log("SC-55: ROM folders %s", list);
#ifdef _WIN32
    _putenv_s(ROM_PATH_ENV, list); /* the plugin shares the C runtime and its environment */
#else
    setenv(ROM_PATH_ENV, list, 1);
#endif
    return true;
}

/* Writes into path the plugin file in folder when it exists. */
static bool plugin_in(const char *folder, char *path, size_t size)
{
    return join_path(path, size, folder, PLUGIN_FILE) && (is_file(path) || is_folder(path));
}

/* Writes into path the plugin file: plugin when given, or the first one in the plugins folder of the game folder,
   of the program folder, or of DOSBox Staging. False with a message when there is none. */
static bool find_plugin(const char *plugin, char *path, size_t size)
{
    if (plugin) {
        int written = snprintf(path, size, "%s", plugin);
        if (written > 0 && (size_t)written < size && (is_file(path) || is_folder(path)))
            return true;
        os_log("SC-55: no plugin %s", plugin);
        return false;
    }
    char folder[OS_PATH_SIZE];
    const char *program = SDL_GetBasePath();
    if (plugin_in(PLUGIN_FOLDER, path, size))
        return true;
    if (program && snprintf(folder, sizeof folder, "%s%s", program, PLUGIN_FOLDER) > 0 && plugin_in(folder, path, size))
        return true;
    for (size_t i = 0; i < SDL_arraysize(plugin_places); i++)
        if (place_folder(&plugin_places[i], folder, sizeof folder) && plugin_in(folder, path, size))
            return true;
    os_log("SC-55: no %s in the plugins folder of the game, of the program, or of DOSBox Staging", PLUGIN_FILE);
    return false;
}

/* Writes into binary the library of a plugin: the plugin file, or the first file in the binary folder of a macOS
   bundle. */
static bool plugin_binary(const char *plugin, char *binary, size_t size)
{
    if (!is_folder(plugin)) {
        int written = snprintf(binary, size, "%s", plugin);
        return written > 0 && (size_t)written < size;
    }
    char folder[OS_PATH_SIZE];
    if (!join_path(folder, sizeof folder, plugin, BUNDLE_BINARY))
        return false;
    int count = 0;
    char **files = SDL_GlobDirectory(folder, NULL, 0, &count);
    bool found = files && count > 0 && join_path(binary, size, folder, files[0]);
    SDL_free(files);
    return found;
}

/* Plugin */

static const void *CLAP_ABI host_extension(const clap_host_t *host, const char *id)
{
    (void)host;
    (void)id;
    return NULL;
}

static void CLAP_ABI host_request(const clap_host_t *host)
{
    (void)host;
}

/* The host that the plugin sees: no extensions, and requests that it ignores. */
static const clap_host_t host = {
    .clap_version = CLAP_VERSION_INIT,
    .name = "DM2Mac",
    .vendor = "DM2Mac",
    .url = "",
    .version = "1.0",
    .get_extension = host_extension,
    .request_restart = host_request,
    .request_process = host_request,
    .request_callback = host_request,
};

/* Loads the plugin library and its entry; false with a message. */
static bool load_library(const char *plugin)
{
    char binary[OS_PATH_SIZE];
    if (!plugin_binary(plugin, binary, sizeof binary) || !(sc55.library = SDL_LoadObject(binary))) {
        os_log("SC-55: cannot load %s: %s", plugin, SDL_GetError());
        return false;
    }
    SDL_FunctionPointer entry = SDL_LoadFunction(sc55.library, "clap_entry");
    sc55.entry = (const clap_plugin_entry_t *)(void *)entry; /* the symbol is a struct, not a function */
    if (!sc55.entry || !clap_version_is_compatible(sc55.entry->clap_version) || !sc55.entry->init(plugin)) {
        os_log("SC-55: %s is no CLAP plugin of this version", plugin);
        sc55.entry = NULL;
        SDL_UnloadObject(sc55.library);
        sc55.library = NULL;
        return false;
    }
    return true;
}

/* Sets up the buffers of the audio ports of one direction; false when they do not fit or the first output is not
   stereo. */
static bool setup_ports(const clap_plugin_t *plugin, const clap_plugin_audio_ports_t *ports, bool input,
                        AudioPorts *out)
{
    uint32_t channel = 0;
    out->count = ports->count(plugin, input);
    if (out->count > MAX_AUDIO_PORTS || (!input && !out->count))
        return false;
    for (uint32_t p = 0; p < out->count; p++) {
        clap_audio_port_info_t info;
        if (!ports->get(plugin, p, input, &info) || !info.channel_count || channel + info.channel_count > MAX_CHANNELS)
            return false;
        if (!input && !p &&
            (info.channel_count != MUSIC_CHANNELS || !info.port_type || strcmp(info.port_type, CLAP_PORT_STEREO)))
            return false;
        out->ports[p] = (clap_audio_buffer_t){.data32 = out->channels + channel, .channel_count = info.channel_count};
        for (uint32_t c = 0; c < info.channel_count; c++, channel++)
            out->channels[channel] = out->samples[channel];
    }
    return true;
}

/* Whether the plugin takes MIDI on its first note port and has the audio ports that the driver can feed. */
static bool can_play(const clap_plugin_t *plugin)
{
    const clap_plugin_note_ports_t *notes = plugin->get_extension(plugin, CLAP_EXT_NOTE_PORTS);
    const clap_plugin_audio_ports_t *audio = plugin->get_extension(plugin, CLAP_EXT_AUDIO_PORTS);
    clap_note_port_info_t info;
    return notes && audio && notes->count(plugin, true) && notes->get(plugin, 0, true, &info) &&
           (info.supported_dialects & CLAP_NOTE_DIALECT_MIDI) && setup_ports(plugin, audio, true, &sc55.inputs) &&
           setup_ports(plugin, audio, false, &sc55.outputs);
}

/* The descriptor of the plugin whose name names the model, or NULL. */
static const clap_plugin_descriptor_t *find_model(const clap_plugin_factory_t *factory, const Model *model)
{
    uint32_t count = factory->get_plugin_count(factory);
    for (uint32_t i = 0; i < count; i++) {
        const clap_plugin_descriptor_t *desc = factory->get_plugin_descriptor(factory, i);
        if (desc && desc->id && desc->name && SDL_strcasestr(desc->name, model->name))
            return desc;
    }
    return NULL;
}

/* Creates and activates the plugin of the first model that its ROMs allow; NULL with a message. */
static const Model *start_model(void)
{
    const clap_plugin_factory_t *factory = sc55.entry->get_factory(CLAP_PLUGIN_FACTORY_ID);
    for (size_t m = 0; factory && m < SDL_arraysize(models); m++) {
        const clap_plugin_descriptor_t *desc = find_model(factory, &models[m]);
        const clap_plugin_t *plugin = desc ? factory->create_plugin(factory, &host, desc->id) : NULL;
        if (!plugin)
            continue;
        if (plugin->init(plugin) && can_play(plugin) &&
            plugin->activate(plugin, models[m].rate, BLOCK_FRAMES, BLOCK_FRAMES)) {
            sc55.plugin = plugin;
            os_log("SC-55: playing on %s", models[m].name);
            return &models[m];
        }
        plugin->destroy(plugin); /* without its ROMs, the init of a model fails */
    }
    os_log("SC-55: the plugin finds no ROMs of an SC-55 model");
    return NULL;
}

/* Render thread */

static uint32_t CLAP_ABI event_count(const clap_input_events_t *list)
{
    (void)list;
    return sc55.event_count + sc55.reset_due;
}

/* The event at index of the block: the GS reset when it is due, then the MIDI messages. */
static const clap_event_header_t *CLAP_ABI event_at(const clap_input_events_t *list, uint32_t index)
{
    (void)list;
    if (sc55.reset_due && !index)
        return &sc55.reset.header;
    index -= sc55.reset_due;
    return index < sc55.event_count ? &sc55.events[index].header : NULL;
}

static bool CLAP_ABI push_event(const clap_output_events_t *list, const clap_event_header_t *event)
{
    (void)list;
    (void)event;
    return true; /* the driver takes no events of the plugin */
}

/* The events of the next block and the events that the plugin sends back. */
static const clap_input_events_t in_events = {NULL, event_count, event_at};
static const clap_output_events_t out_events = {NULL, push_event};

/* Moves the queued MIDI messages into the events of the next block. */
static void take_messages(void)
{
    SDL_LockMutex(sc55.lock);
    for (int i = 0; i < sc55.queued; i++) {
        clap_event_midi_t *event = &sc55.events[i];
        *event = (clap_event_midi_t){
            .header = {.size = sizeof *event, .space_id = CLAP_CORE_EVENT_SPACE_ID, .type = CLAP_EVENT_MIDI}
        };
        memcpy(event->data, sc55.queue[i].bytes, MIDI_EVENT_SIZE);
    }
    sc55.event_count = (uint32_t)sc55.queued;
    sc55.queued = 0;
    SDL_UnlockMutex(sc55.lock);
}

/* Renders one block with the queued messages and puts it into the audio stream. */
static void render_block(void)
{
    take_messages();
    clap_process_t process = {
        .steady_time = -1,
        .frames_count = BLOCK_FRAMES,
        .audio_inputs = sc55.inputs.ports,
        .audio_outputs = sc55.outputs.ports,
        .audio_inputs_count = sc55.inputs.count,
        .audio_outputs_count = sc55.outputs.count,
        .in_events = &in_events,
        .out_events = &out_events,
    };
    bool ok = sc55.plugin->process(sc55.plugin, &process) != CLAP_PROCESS_ERROR;
    sc55.reset_due = false;
    for (int i = 0; i < BLOCK_FRAMES; i++)
        for (int c = 0; c < MUSIC_CHANNELS; c++)
            sc55.stereo[i * MUSIC_CHANNELS + c] = ok ? sc55.outputs.samples[c][i] : 0;
    SDL_PutAudioStreamData(sc55.stream, sc55.stereo, sizeof sc55.stereo);
}

/* Keeps QUEUED_FRAMES of sound in the audio stream until the driver closes. */
static int SDLCALL render_thread(void *unused)
{
    (void)unused;
    const int queued_bytes = QUEUED_FRAMES * MUSIC_CHANNELS * (int)sizeof(int16_t);
    if (!sc55.plugin->start_processing(sc55.plugin)) {
        os_log("SC-55: the plugin does not start: playing silence");
        return 0;
    }
    while (!SDL_GetAtomicInt(&sc55.stop)) {
        if (SDL_GetAudioStreamAvailable(sc55.stream) >= queued_bytes)
            SDL_DelayNS(SDL_NS_PER_MS);
        else
            render_block();
    }
    sc55.plugin->stop_processing(sc55.plugin);
    return 0;
}

/* Driver */

/* Queues the message for the render thread; drops it when the queue is full. */
static void queue_message(uint8_t status, uint8_t data1, uint8_t data2)
{
    SDL_LockMutex(sc55.lock);
    if (sc55.queued < MESSAGE_QUEUE)
        sc55.queue[sc55.queued++] = (Message){
            {status, data1, data2}
        };
    SDL_UnlockMutex(sc55.lock);
}

/* Plays a MIDI message; the HMI pitch bend range becomes the registered parameter of General MIDI. */
static void send(const uint8_t *event)
{
    uint8_t status = event[MIDI_STATUS];
    if ((status & MIDI_STATUS_MASK) != MIDI_CONTROL || event[MIDI_DATA1] != MIDI_CTRL_BEND_RANGE) {
        queue_message(status, event[MIDI_DATA1], event[MIDI_DATA2]);
        return;
    }
    const uint8_t range[BEND_RANGE_SEND][2] = {
        {MIDI_CTRL_RPN_MSB,        RPN_BEND_RANGE   },
        {MIDI_CTRL_RPN_LSB,        RPN_BEND_RANGE   },
        {MIDI_CTRL_DATA_ENTRY,     event[MIDI_DATA2]},
        {MIDI_CTRL_DATA_ENTRY_LSB, 0                },
        {MIDI_CTRL_RPN_MSB,        RPN_NONE         },
        {MIDI_CTRL_RPN_LSB,        RPN_NONE         },
    };
    for (int i = 0; i < BEND_RANGE_SEND; i++)
        queue_message(status, range[i][0], range[i][1]);
}

/* Takes the frames that the render thread has made; silence for the frames it has not made yet. */
static void render(int16_t *out, int frames)
{
    int got = SDL_GetAudioStreamData(sc55.stream, out, frames * MUSIC_CHANNELS * (int)sizeof *out);
    int samples = got > 0 ? got / (int)sizeof *out : 0;
    memset(out + samples, 0, (size_t)(frames * MUSIC_CHANNELS - samples) * sizeof *out);
}

/* The driver that the music player gets. */
static const MusicDriver sc55_driver = {HMP_DEVICE_GM, send, render};

/* Unloads the plugin library; the plugin itself is already destroyed. */
static void unload_library(void)
{
    sc55.entry->deinit();
    SDL_UnloadObject(sc55.library);
    sc55.entry = NULL;
    sc55.library = NULL;
}

/* Makes the audio stream, the lock, and the render thread for the plugin rate; false with a message. */
static bool start_thread(int rate)
{
    const SDL_AudioSpec from = {SDL_AUDIO_F32, MUSIC_CHANNELS, rate};
    const SDL_AudioSpec to = {SDL_AUDIO_S16, MUSIC_CHANNELS, MUSIC_SAMPLE_RATE};
    SDL_SetAtomicInt(&sc55.stop, 0);
    sc55.reset = (clap_event_midi_sysex_t){
        .header = {.size = sizeof sc55.reset, .space_id = CLAP_CORE_EVENT_SPACE_ID, .type = CLAP_EVENT_MIDI_SYSEX},
        .buffer = gs_reset,
        .size = sizeof gs_reset,
    };
    sc55.reset_due = true;
    if ((sc55.stream = SDL_CreateAudioStream(&from, &to)) && (sc55.lock = SDL_CreateMutex()) &&
        (sc55.thread = SDL_CreateThread(render_thread, "sc55", NULL)))
        return true;
    os_log("SC-55: %s", SDL_GetError());
    SDL_DestroyMutex(sc55.lock);
    SDL_DestroyAudioStream(sc55.stream);
    sc55.lock = NULL;
    sc55.stream = NULL;
    return false;
}

/* Setup */

const MusicDriver *sc55_open(Sc55Paths paths)
{
    char plugin[OS_PATH_SIZE];
    if (sc55.plugin || !set_rom_folders(paths.roms) || !find_plugin(paths.plugin, plugin, sizeof plugin) ||
        !load_library(plugin))
        return NULL;
    os_log("SC-55: plugin %s", plugin);
    const Model *model = start_model();
    if (!model) {
        unload_library();
        return NULL;
    }
    if (!start_thread(model->rate)) {
        sc55.plugin->deactivate(sc55.plugin);
        sc55.plugin->destroy(sc55.plugin);
        sc55.plugin = NULL;
        unload_library();
        return NULL;
    }
    return &sc55_driver;
}

void sc55_close(void)
{
    if (!sc55.plugin)
        return;
    SDL_SetAtomicInt(&sc55.stop, 1);
    SDL_WaitThread(sc55.thread, NULL);
    sc55.thread = NULL;
    sc55.plugin->deactivate(sc55.plugin);
    sc55.plugin->destroy(sc55.plugin);
    sc55.plugin = NULL;
    unload_library();
    SDL_DestroyAudioStream(sc55.stream);
    SDL_DestroyMutex(sc55.lock);
    sc55.stream = NULL;
    sc55.lock = NULL;
}
