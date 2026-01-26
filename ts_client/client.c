#include <teamspeak/public_definitions.h>
#include <teamspeak/public_errors.h>
#include <teamspeak/clientlib.h>

#include <stdbool.h>
#include <stdio.h>

const char* animals[] = {
    "Wolf", "Fox", "Bear", "Eagle", "Hawk", "Falcon", "Tiger", "Lion",
    "Panther", "Jaguar", "Leopard", "Cobra", "Viper", "Python", "Dragon",
    "Phoenix", "Griffin", "Raven", "Crow", "Owl", "Shark", "Whale",
    "Dolphin", "Orca", "Octopus", "Squid", "Mantis", "Scorpion", "Spider",
    "Beetle", "Hornet", "Wasp", "Lynx", "Puma", "Cheetah", "Rhino",
    "Bison", "Moose", "Elk", "Stag", "Badger", "Wolverine", "Otter",
    "Beaver", "Raccoon", "Coyote", "Jackal", "Hyena", "Gorilla", "Chimp",
    "Mandrill", "Lemur", "Sloth", "Armadillo", "Porcupine", "Hedgehog",
    "Penguin", "Pelican", "Flamingo", "Heron", "Crane", "Stork", "Condor",
    "Vulture", "Parrot", "Macaw", "Toucan", "Kingfisher", "Sparrow", "Finch",
    "Cardinal", "Robin", "Wren", "Swift", "Swallow", "Hummingbird", "Peacock",
    "Pheasant", "Quail", "Grouse", "Turkey", "Goose", "Swan", "Duck",
    "Gator", "Croc", "Iguana", "Gecko", "Chameleon", "Turtle", "Tortoise",
    "Frog", "Toad", "Newt", "Salamander", "Axolotl", "Barracuda", "Marlin",
    "Swordfish", "Tuna", "Salmon", "Trout", "Pike", "Bass", "Carp"
};

const char* colors[] = {
    "Red", "Blue", "Green", "Yellow", "Orange", "Purple", "Violet",
    "Pink", "Cyan", "Magenta", "Crimson", "Scarlet", "Ruby", "Coral",
    "Salmon", "Peach", "Amber", "Gold", "Bronze", "Copper", "Brass",
    "Lime", "Emerald", "Jade", "Mint", "Teal", "Turquoise", "Aqua",
    "Azure", "Cobalt", "Navy", "Indigo", "Sapphire", "Lavender", "Plum",
    "Grape", "Orchid", "Fuchsia", "Rose", "Cherry", "Maroon", "Burgundy",
    "Silver", "Platinum", "Chrome", "Steel", "Iron", "Slate", "Charcoal",
    "Onyx", "Obsidian", "Jet", "Ivory", "Pearl", "Cream", "Vanilla",
    "Snow", "Frost", "Ice", "Ash", "Smoke", "Shadow", "Midnight",
    "Raven", "Ebony", "Coal", "Noir", "Storm", "Thunder", "Lightning",
    "Flame", "Ember", "Blaze", "Inferno", "Solar", "Lunar", "Cosmic",
    "Stellar", "Neon", "Electric", "Atomic", "Crystal", "Prism", "Opal",
    "Topaz", "Garnet", "Onyx", "Jasper", "Moss", "Forest", "Ocean",
    "Arctic", "Desert", "Sunset", "Dawn", "Dusk", "Twilight", "Aurora"
};

// Global counter for received voice packets
static unsigned long long voicePacketsReceived = 0;


bool ValidateInput();
void UserInput();


void onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber) {
    if(newStatus == STATUS_CONNECTION_ESTABLISHED)
        puts("Connected to server");
    else if(newStatus == STATUS_DISCONNECTED)
        puts("Disconnected from server");
}

void onTalkStatusChangeEvent(uint64 serverConnectionHandlerID, int status, int isReceivedWhisper, anyID clientID) {
    char* name;
    if(ts3client_getClientVariableAsString(serverConnectionHandlerID, clientID, CLIENT_NICKNAME, &name) == ERROR_ok) {
        if(status == STATUS_TALKING) {
            printf("%s is talking\n", name);
        } else {
            printf("%s stopped talking\n", name);
            printf("Voice packets received: %llu\n", voicePacketsReceived);
        }
        ts3client_freeMemory(name);
    }
}

void onEditPlaybackVoiceDataEvent(uint64 serverConnectionHandlerID, anyID clientID, short* samples, int sampleCount, int channels) {
    voicePacketsReceived++;
}

bool FreeMemory(char* identity, uint64 scHandlerID) {
    if(identity != NULL)
        ts3client_freeMemory(identity);

    ts3client_stopConnection(scHandlerID, "leaving");
    ts3client_destroyServerConnectionHandler(scHandlerID);
    ts3client_destroyClientLib();
    return true;
}

void generate_nickname(char* buffer) {
    const int num_animals = sizeof(animals) / sizeof(animals[0]);
    const int num_colors = sizeof(colors) / sizeof(colors[0]);

    const char* color = colors[rand() % num_colors];
    const char* animal = animals[rand() % num_animals];
    
    sprintf(buffer, "%s %s", color, animal);
}

int main(int argc, char **argv) {
    uint64 scHandlerID;
    unsigned int error;
    char *identity = NULL;
    char nickname[30];
    bool isMuted = false;
    struct ClientUIFunctions funcs;

    // temporary solution
    char ip_address[16];
    unsigned int port;
    char buffer[6];
    char password[30];

    srand(time(NULL));
    memset(&funcs, 0, sizeof(struct ClientUIFunctions));
    generate_nickname(nickname);

    puts("Input ip address:");
    fgets(ip_address, sizeof(ip_address), stdin);
    ip_address[strcspn(ip_address, "\n")] = '\0';

    puts("Input port:");
    fgets(buffer, sizeof(buffer), stdin);
    port = atoi(buffer);

    puts("Input password:");
    fgets(password, sizeof(password), stdin);
    password[strcspn(password, "\n")] = '\0';
    // --------------------------------------

    // Setup callbacks
    funcs.onConnectStatusChangeEvent = onConnectStatusChangeEvent;
    funcs.onTalkStatusChangeEvent = onTalkStatusChangeEvent;
    funcs.onEditPlaybackVoiceDataEvent = onEditPlaybackVoiceDataEvent;

    // Initialize client library (no logs)
    if(ts3client_initClientLib(&funcs, NULL, LogType_NONE, NULL, "") != ERROR_ok) {
        puts("Failed to initialize client library");
        return -1;
    }

    if(ts3client_spawnNewServerConnectionHandler(0, &scHandlerID) != ERROR_ok) {
        puts("Failed to spawn connection handler");
        ts3client_destroyClientLib();
        return -1;
    }

    // Open audio devices
    if(ts3client_openCaptureDevice(scHandlerID, "", NULL) != ERROR_ok) {
        puts("ERROR: Failed to open microphone");
        ts3client_destroyServerConnectionHandler(scHandlerID);
        ts3client_destroyClientLib();
        return -1;
    }

    if(ts3client_openPlaybackDevice(scHandlerID, "", NULL) != ERROR_ok) {
        puts("ERROR: Failed to open speakers");
        ts3client_destroyServerConnectionHandler(scHandlerID);
        ts3client_destroyClientLib();
        return -1;
    }

    // Configure VAD
    ts3client_setPreProcessorConfigValue(scHandlerID, "vad", "true");
    ts3client_setPreProcessorConfigValue(scHandlerID, "vad_mode", "2");
    ts3client_setPreProcessorConfigValue(scHandlerID, "voiceactivation_level", "-20");

    // Create identity
    if(ts3client_createIdentity(&identity) != ERROR_ok) {
        puts("Failed to create identity");
        ts3client_destroyServerConnectionHandler(scHandlerID);
        ts3client_destroyClientLib();
        return -1;
    }

    // Connect to server
    puts("Connecting to server");
    if(ts3client_startConnection(scHandlerID, identity, ip_address, port, nickname, NULL, "", password) != ERROR_ok) {
        puts("Failed to connect");
        ts3client_freeMemory(identity);
        ts3client_destroyServerConnectionHandler(scHandlerID);
        ts3client_destroyClientLib();
        return -1;
    }

    Sleep(1000);
    ts3client_setClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_DEACTIVATED, INPUT_ACTIVE);
    ts3client_setClientSelfVariableAsInt(scHandlerID, CLIENT_INPUT_MUTED, 0);
    ts3client_flushClientSelfUpdates(scHandlerID, NULL);
    getchar();


    FreeMemory(identity, scHandlerID);
    free(ip_address);
    free(password);
    return 0;
}
