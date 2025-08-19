#include <ServoEasing.hpp>
ServoEasing servo1;
ServoEasing servo2;

#include <WiFi.h>
#include <esp_http_server.h>
#include <esp_camera.h>

// WiFi
const char *ssid     = "XXXXXXXX";
const char *password = "XXXXXXXX";
const int LOCAL_IP[4] = {192, 168,   0, 120};
const int SUBNET[4]   = {255, 255, 255,   0};
const int GATEWAY[4]  = {192, 168,   0,   1};
const int DNS_ADDR[4] = {192, 168,   0,   1};

// HTML file content
#include "html_text.h" 

// For Motion JPEG streaming
#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

uint8_t* out_jpg   = NULL;
size_t out_jpg_len = 0;

void setup() {
  Serial.begin(115200);
  Serial.println("ATOMS3R-CAM");

  // Servo
  servo1.attach(5);
  servo2.attach(6);
  servo1.easeTo(90, 1000);
  servo2.easeTo(90, 1000);

  // Turn ON G18 (POWER_N)
  pinMode(18, OUTPUT);
  digitalWrite(18, LOW);
  delay(500); // If not delayed, the camera will fail

  // Camera parameters
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       =  3; // Y2
  config.pin_d1       = 42; // Y3
  config.pin_d2       = 46; // Y4
  config.pin_d3       = 48; // Y5
  config.pin_d4       =  4; // Y6
  config.pin_d5       = 17; // Y7
  config.pin_d6       = 11; // Y8
  config.pin_d7       = 13; // Y9
  config.pin_xclk     = 21; // XCLK
  config.pin_pclk     = 40; // PCLK
  config.pin_vsync    = 10; // VSYNC
  config.pin_href     = 14; // HREF
  config.pin_sccb_sda = 12; // SIOD
  config.pin_sccb_scl =  9; // SIOC
  config.pin_pwdn     = -1;
  config.pin_reset    = -1;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_RGB565;
  config.frame_size   = FRAMESIZE_QVGA; // 320x240
  config.jpeg_quality = 30;
  config.fb_count     = 2;
  config.grab_mode    = CAMERA_GRAB_LATEST;
  config.fb_location  = CAMERA_FB_IN_PSRAM;

  // Start camera
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  // Disable image flip
  sensor_t *s = esp_camera_sensor_get();
  s->set_hmirror(s, 0);

  // WiFi
  IPAddress ip(LOCAL_IP[0], LOCAL_IP[1], LOCAL_IP[2], LOCAL_IP[3]);
  IPAddress subnet(SUBNET[0], SUBNET[1], SUBNET[2], SUBNET[3]);
  IPAddress gateway(GATEWAY[0], GATEWAY[1], GATEWAY[2], GATEWAY[3]);
  IPAddress dns(DNS_ADDR[0], DNS_ADDR[1], DNS_ADDR[2], DNS_ADDR[3]);
  if (!WiFi.config(ip, gateway, subnet, dns)){
    Serial.println("Failed to configure!");
  }
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println(".");
  }
  Serial.println(WiFi.localIP());

  // Start web server
  start_MJPEG_server();
}

void loop() {
}

// Start web server
void start_MJPEG_server() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.uri_match_fn = httpd_uri_match_wildcard; // Enable wildcard (*) in URI
  Serial.printf("Starting web server on port: '%d'\n", config.server_port);

  // Document root handler
  httpd_uri_t index_uri = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_handler,
    .user_ctx  = NULL
  };

  // Button handler (button_xxx)
  httpd_uri_t button_uri = {
    .uri       = "/button_*",
    .method    = HTTP_GET,
    .handler   = button_handler,
    .user_ctx  = NULL
  };

  // Start & register handlers
  httpd_handle_t httpd = NULL;
  if (httpd_start(&httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(httpd, &index_uri);  // Document root
    httpd_register_uri_handler(httpd, &button_uri); // Buttons
  }
  
  // Motion JPEG handler
  httpd_uri_t stream_uri = {
    .uri       = "/stream",
    .method    = HTTP_GET,
    .handler   = stream_handler,
    .user_ctx  = NULL
  };

  // Start & register handler
  httpd_handle_t stream_httpd = NULL;
  config.server_port += 1;
  config.ctrl_port += 1;
  Serial.printf("Starting stream server on port: '%d'\n", config.server_port);
  if (httpd_start(&stream_httpd, &config) == ESP_OK) {
    httpd_register_uri_handler(stream_httpd, &stream_uri);
  }
}

// Handler when document root is accessed in browser (return HTML)
static esp_err_t index_handler(httpd_req_t *req) {
  httpd_resp_set_type(req, "text/html");
  httpd_resp_set_hdr(req, "Accept-Charset", "UTF-8");
  return httpd_resp_send(req, HTML_TEXT.c_str(), HTML_TEXT.length());
}

int auto_flag = 0;

// Handler when a button is pressed (does not return anything to browser)
static esp_err_t button_handler(httpd_req_t *req) {
  
  // Branch based on received URI
  String uri = req->uri;
  if(uri == "/button_left") {
    move_lr(10);
  }
  else if(uri == "/button_right") {
    move_lr(-10);
  }
  else if(uri == "/button_up") {
    move_ud(10);
  }
  else if(uri == "/button_down") {
    move_ud(-10);
  }
  else if(uri == "/button_auto") {
    auto_flag = 1;
  }
  else if(uri == "/button_stop") {
    auto_flag = 0;
  }
  return httpd_resp_send(req, NULL, 0);
}

// Servo angle control
int lr = 90;
int ud = 90;
int lr_min =   0;
int lr_max = 180;
int ud_min = 50;
int ud_max = 120;
void move_lr(int x) {
    lr = lr + x;
    if(lr > lr_max) lr = lr_max;
    servo1.easeTo(lr, 200);
}
void move_ud(int y) {
    ud = ud - y;
    if(ud > ud_max) ud = ud_max;
    servo2.easeTo(ud, 200);
}

// Handler when Motion JPEG (<img>) is accessed in browser (loop sending until closed)
static esp_err_t stream_handler(httpd_req_t *req) {

  camera_fb_t *fb = NULL;
  esp_err_t res = ESP_OK;
  char * part_buf[64];

  // Send HTTP header (for entire stream)
  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }

  while (true) {

    // Capture frame
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("Camera capture failed");
      res = ESP_FAIL;
    }

    // Auto control
    if (auto_flag == 1) {
      // Color detection
      int count[] = {0, 0, 0, 0};
      int height = fb->height;
      int width  = fb->width;
      int y_min = 1000, y_max = -1000;
      int x_min = 1000, x_max = -1000;
      for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
          size_t pixel_index = (y * width + x) * 2;
          uint16_t pixel = (fb->buf[pixel_index] << 8) | (fb->buf[pixel_index + 1]);
          uint8_t r = ((pixel >> 11) & 0x1F) << 3; // Red
          uint8_t g = ((pixel >> 5) & 0x3F)  << 2; // Green
          uint8_t b = (pixel & 0x1F)         << 3; // Blue

          if (r > 200 && g < 100 && b < 100) {

            if      (x < width/3)    count[0]++; // Left
            else if (x > width/3*2)  count[1]++; // Right
            if      (y < height/3)   count[2]++; // Up
            else if (y > height/3*2) count[3]++; // Down

            // Detection frame
            if (x_min > x) x_min = x;
            else if (x_max < x) x_max = x;
            if (y_min > y) y_min = y;
            else if (y_max < y) y_max = y;

            drwa_red(fb, pixel_index);
          }
        }
      }

      // Draw detection box
      for (int x = x_min; x <= x_max; x++) {
        size_t pixel_index = (y_min * width + x) * 2;
        drwa_red(fb, pixel_index);
        pixel_index = (y_max * width + x) * 2;
        drwa_red(fb, pixel_index);
      }
      for (int y = y_min; y <= y_max; y++) {
        size_t pixel_index = (y * width + x_min) * 2;
        drwa_red(fb, pixel_index);
        pixel_index = (y * width + x_max) * 2;
        drwa_red(fb, pixel_index);
      }

      // Movement
      if      ((count[0] > count[1]) && (count[0] > 100)) move_lr( 5); // Left
      else if ((count[0] < count[1]) && (count[1] > 100)) move_lr(-5); // Right
      if      ((count[2] > count[3]) && (count[2] > 100)) move_ud( 5); // Up
      else if ((count[2] < count[3]) && (count[3] > 100)) move_ud(-5); // Down
    }

    // JPEG conversion
    free(out_jpg);
    bool jpeg_converted = frame2jpg(fb, 80, &out_jpg, &out_jpg_len);
    esp_camera_fb_return(fb);
    fb = NULL;
    if (!jpeg_converted) {
      Serial.println("JPEG compression failed");
      res = ESP_FAIL;
    }

    // Send HTTP header (for each jpeg frame)
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, out_jpg_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }

    // Send JPEG data
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)out_jpg, out_jpg_len);
    }

    // Send boundary (data separator)
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }

    if (fb) {
      esp_camera_fb_return(fb);
      out_jpg = NULL;
    }

    // Exit when disconnected or error
    if (res != ESP_OK) {
      break;
    }
  }
  return res;
}

// Draw a red pixel
void drwa_red(camera_fb_t *fb, size_t pixel_index) {
    uint8_t r = 255, g = 0, b = 0; // Red
    uint16_t new_pixel = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    fb->buf[pixel_index + 1] = new_pixel & 0xFF;
    fb->buf[pixel_index] = (new_pixel >> 8) & 0xFF;
}
