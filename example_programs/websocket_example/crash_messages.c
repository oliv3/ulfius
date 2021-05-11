#include <string.h>
#include <unistd.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <ulfius.h>

#define PORT 9275
#define PREFIX_WEBSOCKET "/websocket%d"
#define WEBSOCKETS 10
#define INSTANCES 10

#if defined(U_DISABLE_WEBSOCKET)

int main() {
  fprintf(stderr, "Websocket not supported, please recompile ulfius with websocket support\n");
  return 1;
}

#else

static char * read_file(const char * filename, long *length) {
  char * buffer = NULL;
  FILE * f;
  if (filename != NULL) {
    f = fopen (filename, "rb");
    if (f) {
      fseek (f, 0, SEEK_END);
      *length = ftell (f);
      fseek (f, 0, SEEK_SET);
      buffer = o_malloc (*length + 1);
      if (buffer) {
        fread (buffer, 1, *length, f);
        buffer[*length] = '\0';
      }
      fclose (f);
    }
    return buffer;
  } else {
    return NULL;
  }
}


int callback_websocket (const struct _u_request * request, struct _u_response * response, void * user_data);

#define PNG_FILE "lebiniou.png"
static char *png_data = NULL;
static long png_len = -1;

/**
 * main function
 * open the wbservice on port 9275
 */
int main(int argc, char ** argv) {
  int ret;
  struct _u_instance instance[INSTANCES];
  
  y_init_logs("crash_messages", Y_LOG_MODE_CONSOLE, Y_LOG_LEVEL_DEBUG, NULL, "Starting crash_messages");

  png_data = read_file(PNG_FILE, &png_len);
  y_log_message(Y_LOG_LEVEL_INFO, "Read %s: %ld bytes", PNG_FILE, png_len);

  for (int8_t i = 0; i < INSTANCES; ++i) {
    if (ulfius_init_instance(&instance[i], PORT + i, NULL, NULL) != U_OK) {
      y_log_message(Y_LOG_LEVEL_ERROR, "Error ulfius_init_instance, abort");
      return(1);
    }
    
    u_map_put(instance[i].default_headers, "Access-Control-Allow-Origin", "*");
    
    // Endpoint list declaration
    for (int8_t j = 0; j < WEBSOCKETS; ++j) {
      char *name = msprintf(PREFIX_WEBSOCKET, j);
      y_log_message(Y_LOG_LEVEL_INFO, "Create endpoint '%s'", name);
      ulfius_add_endpoint_by_val(&instance[i], "GET", name, NULL, 0, &callback_websocket, NULL);
      o_free(name);
    }

    // Start the framework
    ret = ulfius_start_framework(&instance[i]);
    
    if (ret == U_OK) {
      y_log_message(Y_LOG_LEVEL_INFO, "Start framework on port %d", instance[i].port);
    } else {
      y_log_message(Y_LOG_LEVEL_ERROR, "Error starting framework");
    }
  }

  // Wait for the user to press <enter> on the console to quit the application
  getchar();
  y_log_message(Y_LOG_LEVEL_INFO, "End framework");
    
  for (int8_t i = 0; i < INSTANCES; ++i) {
    for (int8_t j = 0; j < WEBSOCKETS; ++j) {
      char *name = msprintf(PREFIX_WEBSOCKET, i);
      ulfius_remove_endpoint_by_val(&instance[i], "GET", name, NULL);
      o_free(name);
    }
    ulfius_stop_framework(&instance[i]);
    ulfius_clean_instance(&instance[i]);
  }

  y_close_logs();
  
  return 0;
}


void websocket_onclose_callback (const struct _u_request * request,
                                struct _websocket_manager * websocket_manager,
                                void * websocket_onclose_user_data) {
}

void websocket_manager_callback(const struct _u_request * request,
                               struct _websocket_manager * websocket_manager,
                               void * websocket_manager_user_data) {
  while (1) { // ulfius_websocket_wait_close(websocket_manager, 2000) == U_WEBSOCKET_STATUS_OPEN) {
    if (ulfius_websocket_send_message(websocket_manager, U_WEBSOCKET_OPCODE_BINARY, png_len, png_data) != U_OK) {
      y_log_message(Y_LOG_LEVEL_ERROR, "Error send binary message without fragmentation");
    }
    ulfius_clear_websocket_message(ulfius_websocket_pop_first_message(websocket_manager->message_list_outcoming));
  }

  y_log_message(Y_LOG_LEVEL_DEBUG, "Closing websocket_manager_callback");
}


void websocket_incoming_message_callback (const struct _u_request * request,
                                         struct _websocket_manager * websocket_manager,
                                         const struct _websocket_message * last_message,
                                         void * websocket_incoming_message_user_data) {
  if (last_message->opcode == U_WEBSOCKET_OPCODE_TEXT) {
    y_log_message(Y_LOG_LEVEL_DEBUG, "text payload '%.*s'", last_message->data_len, last_message->data);
  }
  ulfius_clear_websocket_message(ulfius_websocket_pop_first_message(websocket_manager->message_list_incoming));
}


int callback_websocket (const struct _u_request * request, struct _u_response * response, void * user_data) {
  char * websocket_user_data = NULL;
  int ret;
  
  if ((ret = ulfius_set_websocket_response(response, NULL, NULL, &websocket_manager_callback, websocket_user_data, &websocket_incoming_message_callback, websocket_user_data, &websocket_onclose_callback, websocket_user_data)) == U_OK) {
    ulfius_add_websocket_deflate_extension(response);
    return U_CALLBACK_CONTINUE;
  } else {
    return U_CALLBACK_ERROR;
  }
}
#endif
