#include <teamspeak/public_definitions.h>
#include <teamspeak/public_errors.h>
#include <teamspeak/clientlib.h>

#include <stdint.h>

int8_t connection_status;
char* identity = NULL;


void ts_init(const char* soundbackends_path) {
    struct ClientUIFunctions funcs;
    memset(&funcs, 0, sizeof(struct ClientUIFunctions));

    funcs.onConnectStatusChangeEvent = onConnectStatusChangeEvent;

    ts3client_initClientLib(&funcs, NULL, LogType_FILE | LogType_CONSOLE, NULL, soundbackends_path);
    ts3client_createIdentity(&identity);
}

uint64 ts_connect() {
    uint64 connection_id = 0;
    ts3client_spawnNewServerConnectionHandler(0, &connection_id);
    ts3client_startConnection(connection_id, identity, "localhost", 9987, "client", NULL, "", "secret");
    return connection_id;
}

void start_audio(uint64 connection_id) {
    ts3client_openPlaybackDevice(connection_id, "", NULL);
    ts3client_openCaptureDevice(connection_id, "", NULL);
}

void stop_audio(uint64 connection_id) {
    ts3client_closeCaptureDevice(connection_id);
    ts3client_closePlaybackDevice(connection_id);
}

void onConnectStatusChangeEvent(uint64 connection_id, int new_status, unsigned int connection_error) {
    if (new_status == STATUS_DISCONNECTED) {
        if (connection_error == ERROR_failed_connection_initialisation) {
            printf("Looks like there is no server running.\n");
        }
    }

    connection_status = new_status;
}


int main(int argc, char **argv) {
    ts_init(program_path(argv[0]));
    printf("Initializing success");

    const uint64 connection_id = ts_connect();
    if (connection_id != 0) {
        start_audio(connection_id);
        printf("Starting audio success");

        if (wait_for_connection()) {
            printf("\n--- Press Return to disconnect from server and exit ---\n");
            getchar();

            ts3client_stopConnection(connection_id, "leaving");

            while (connection_status != STATUS_DISCONNECTED)
                sleep_ms(20);
        }

        stop_audio(connection_id);
        printf("Stopping audio success");
        ts3client_destroyServerConnectionHandler(connection_id);
    }
    ts_shutdown();
    return 0;
}