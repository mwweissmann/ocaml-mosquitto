#include <assert.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/if.h>

#include <string.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>

#include <mosquitto.h>

#define CAML_NAME_SPACE
#include <caml/mlvalues.h>
#include <caml/memory.h>
#include <caml/alloc.h> 
#include <caml/threads.h> 
#include <caml/callback.h>
#include <caml/fail.h>
#include <caml/unixsupport.h>

#define RESULT_OK caml_alloc(1, 0)
#define RESULT_ERROR caml_alloc(1, 1)

static value eunix;

struct m {
  struct mosquitto *conn;
  char uid[64];
};

CAMLprim value mqtt_initialize(value unit) {
  CAMLparam1(unit);
  CAMLlocal1(result);
  int version_number, major, minor, revision;

  eunix = caml_hash_variant("EUnix");
  mosquitto_lib_init();
  version_number = mosquitto_lib_version(&major, &minor, &revision);

  result = caml_alloc_tuple(4);
  Store_field(result, 0, Val_int(version_number));
  Store_field(result, 1, Val_int(major));
  Store_field(result, 2, Val_int(minor));
  Store_field(result, 3, Val_int(revision));
  CAMLreturn(result);
}

CAMLprim value mqtt_create(value id, value clean_session) {
  CAMLparam2(id, clean_session);
  CAMLlocal4(result, mqtt, perrno, uid);

  struct m *mosq;
  int lerrno;
  char *id_;
  bool clean_session_;
  size_t id_len;

  mosq = calloc(1, sizeof(struct m));

  id_len = caml_string_length(id);
  id_ = calloc(1, 1 + id_len);
  memcpy(id_, String_val(id), id_len);
  id_[id_len] = '\0';

  clean_session_ = Bool_val(clean_session);

  caml_release_runtime_system();

  // create unique string for callbacks
  snprintf(mosq->uid, 64, "%p", (void*)mosq);

  if (NULL == (mosq->conn = mosquitto_new(id_, clean_session_, mosq))) {
    lerrno = errno;
  }
  free(id_);

  caml_acquire_runtime_system();

  if (NULL != mosq) {
    uid = caml_alloc_string(strlen(mosq->uid));
    memcpy(String_val(uid), mosq->uid, strlen(mosq->uid));

    mqtt = caml_alloc_tuple(2);
    Store_field(mqtt, 0, (value)mosq);
    Store_field(mqtt, 1, uid);

    result = RESULT_OK;
    Store_field(result, 0, mqtt);
  } else {
    perrno = caml_alloc(2, 0);
    Store_field(perrno, 0, eunix); // `EUnix
    Store_field(perrno, 1, unix_error_of_code(lerrno));

    result = RESULT_ERROR;
    Store_field(result, 0, perrno);
  }

  CAMLreturn(result);
}

CAMLprim value mqtt_connect(value mqtt, value host, value port, value keepalive) {
  CAMLparam4(mqtt, host, port, keepalive);
  CAMLlocal2(result, perrno);

  struct m *mosq;
  char *host_;
  size_t len = caml_string_length(host);
  int lerrno;
  host_ = calloc(1, 1 + len);
  memcpy(host_, String_val(host), len);
  host_[len] = '\0';

  int port_ = Long_val(port);
  int keepalive_ = Long_val(keepalive);
  int rc;

  mosq = (struct m*)mqtt;

  caml_release_runtime_system();

  if (MOSQ_ERR_SUCCESS != (rc = mosquitto_connect(mosq->conn, host_, port_, keepalive_))) {
    lerrno = errno;
  }
  free(host_);

  caml_acquire_runtime_system();

  if (MOSQ_ERR_SUCCESS == rc) {
    result = RESULT_OK;
    Store_field(result, 0, Val_unit);
  } else {
    if (MOSQ_ERR_INVAL == rc) {
      lerrno = EINVAL;
    }
    perrno = caml_alloc(2, 0);
    Store_field(perrno, 0, eunix); // `EUnix
    Store_field(perrno, 1, unix_error_of_code(lerrno));

    result = RESULT_ERROR;
    Store_field(result, 0, perrno);
  }

  CAMLreturn(result);
}

CAMLprim value mqtt_reconnect(value mqtt) {
  CAMLparam1(mqtt);
  CAMLlocal2(result, perrno);

  struct m *mosq;
  int lerrno, rc;

  mosq = (struct m*)mqtt;

  caml_release_runtime_system();

  if (MOSQ_ERR_SUCCESS != (rc = mosquitto_reconnect(mosq->conn))) {
    lerrno = errno;
  }

  caml_acquire_runtime_system();

  if (MOSQ_ERR_SUCCESS == rc) {
    result = RESULT_OK;
    Store_field(result, 0, Val_unit);
  } else {
    if (MOSQ_ERR_INVAL == rc) {
      lerrno = EINVAL;
    }
    perrno = caml_alloc(2, 0);
    Store_field(perrno, 0, eunix); // `EUnix
    Store_field(perrno, 1, unix_error_of_code(lerrno));

    result = RESULT_ERROR;
    Store_field(result, 0, perrno);
  }

  CAMLreturn(result);
}

CAMLprim value mqtt_publish(value mqtt, value topic, value payload, value qos, value retain) {
  CAMLparam5(mqtt, topic, payload, qos, retain);
  CAMLlocal2(result, perrno);

  char *topic_, *payload_;
  size_t topic_len, payload_len;
  struct m *mosq;

  int qos_ = Long_val(qos);
  bool retain_ = Bool_val(retain);
  int rc, lerrno;

  mosq = (struct m*)mqtt;

  topic_len = caml_string_length(topic);
  topic_ = calloc(1, topic_len + 1);
  memcpy(topic_, String_val(topic), topic_len);
  topic_[topic_len] = '\0';

  payload_len = caml_string_length(payload);
  payload_ = calloc(1, payload_len);
  memcpy(payload_, String_val(payload), payload_len);

  caml_release_runtime_system();

  rc = mosquitto_publish(mosq->conn, NULL, topic_, payload_len, payload_, qos_, retain_);
  free(topic_);
  free(payload_);

  caml_acquire_runtime_system();

  if (MOSQ_ERR_SUCCESS == rc) {
    result = RESULT_OK;
    Store_field(result, 0, Val_unit);
  } else {
    switch (rc) {
      case MOSQ_ERR_INVAL: lerrno = EINVAL; break;
      case MOSQ_ERR_NOMEM: lerrno = ENOMEM; break;
      case MOSQ_ERR_NO_CONN: lerrno = ENOTCONN; break;
      case MOSQ_ERR_PROTOCOL: lerrno = EPROTOTYPE; break;
      case MOSQ_ERR_PAYLOAD_SIZE: lerrno = EMSGSIZE; break;
      default: assert(false);
    }
    perrno = caml_alloc(2, 0);
    Store_field(perrno, 0, eunix); // `EUnix
    Store_field(perrno, 1, unix_error_of_code(lerrno));

    result = RESULT_ERROR;
    Store_field(result, 0, perrno);
  }

  CAMLreturn(result);
}

CAMLprim value mqtt_subscribe(value mqtt, value topic, value qos) {
  CAMLparam3(mqtt, topic, qos);
  CAMLlocal2(perrno, result);

  char *topic_;
  size_t topic_len;
  struct m *mosq;
  int rc, lerrno;

  int qos_ = Long_val(qos);
  mosq = (struct m*)mqtt;

  topic_len = caml_string_length(topic);
  topic_ = calloc(1, topic_len + 1);
  memcpy(topic_, String_val(topic), topic_len);
  topic_[topic_len] = '\0';

  caml_release_runtime_system();

  rc = mosquitto_subscribe(mosq->conn, NULL, topic_, qos_);
  free(topic_);

  caml_acquire_runtime_system();

  if (MOSQ_ERR_SUCCESS == rc) {
    result = RESULT_OK;
    Store_field(result, 0, Val_unit);
  } else {
    switch (rc) {
      case MOSQ_ERR_INVAL: lerrno = EINVAL; break;
      case MOSQ_ERR_NOMEM: lerrno = ENOMEM; break;
      case MOSQ_ERR_NO_CONN: lerrno = ENOTCONN; break;
      default: assert(false);
    }
    perrno = caml_alloc(2, 0);
    Store_field(perrno, 0, eunix); // `EUnix
    Store_field(perrno, 1, unix_error_of_code(lerrno));

    result = RESULT_ERROR;
    Store_field(result, 0, perrno);
  }

  CAMLreturn(result);
}

void mqtt_callback_msg(struct mosquitto *mqtt, void *cb, const struct mosquitto_message *msg_) {
  static value * f = NULL;
  struct m * mosq = (struct m*)cb;

  caml_acquire_runtime_system();

  value msg = caml_alloc_tuple(5);

  value payload = caml_alloc_string(msg_->payloadlen); // payload
  memcpy(String_val(payload), msg_->payload, msg_->payloadlen);

  size_t topic_len = strlen(msg_->topic);
  value topic = caml_alloc_string(topic_len);
  memcpy(String_val(topic), msg_->topic, topic_len);

  Store_field(msg, 0, Val_int(msg_->mid));
  Store_field(msg, 1, topic);
  Store_field(msg, 2, payload);
  Store_field(msg, 3, Val_int(msg_->qos));
  Store_field(msg, 4, Val_bool(msg_->retain));

  if (NULL == f) {
    f = caml_named_value(mosq->uid);
  }

  caml_callback(*f, msg);

  caml_release_runtime_system();
}

CAMLprim value mqtt_message_callback_set(value mqtt) {
  CAMLparam1(mqtt);

  struct m* mosq;
  mosq = (struct m*)mqtt;

  caml_release_runtime_system();

  mosquitto_message_callback_set(mosq->conn, &mqtt_callback_msg);

  caml_acquire_runtime_system();

  CAMLreturn(Val_unit);
}

CAMLprim value mqtt_loop(value mqtt, value timeout, value max_packets) {
  CAMLparam3(mqtt, timeout, max_packets);
  CAMLlocal2(perrno, result);

  struct m* mosq;
  int timeout_, max_packets_, lerrno, rc;
  mosq = (struct m*)mqtt;
  timeout_ = Long_val(timeout);

  max_packets_ = Long_val(max_packets);

  caml_release_runtime_system();

  if (MOSQ_ERR_SUCCESS != (rc = mosquitto_loop(mosq->conn, timeout_, max_packets_))) {
    lerrno = errno;
  }

  caml_acquire_runtime_system();

  if (MOSQ_ERR_SUCCESS == rc) {
    result = RESULT_OK;
    Store_field(result, 0, Val_unit);
  } else {
    switch (rc) {
      case MOSQ_ERR_INVAL: lerrno = EINVAL; break;
      case MOSQ_ERR_NOMEM: lerrno = ENOMEM; break;
      case MOSQ_ERR_NO_CONN: lerrno = ENOTCONN; break;
      case MOSQ_ERR_CONN_LOST: lerrno =  ENOTCONN; break;
      case MOSQ_ERR_PROTOCOL: lerrno = EPROTOTYPE; break;
      case MOSQ_ERR_ERRNO: break;
      default: assert(false);
    }
    perrno = caml_alloc(2, 0);
    Store_field(perrno, 0, eunix); // `EUnix
    Store_field(perrno, 1, unix_error_of_code(lerrno));

    result = RESULT_ERROR;
    Store_field(result, 0, perrno);
  }

  CAMLreturn(result);
}

CAMLprim value mqtt_loop_forever(value mqtt, value timeout, value max_packets) {
  CAMLparam3(mqtt, timeout, max_packets);
  CAMLlocal2(perrno, result);

  struct m* mosq;
  int timeout_, max_packets_, lerrno, rc;
  mosq = (struct m*)mqtt;
  timeout_ = Long_val(timeout);

  max_packets_ = Long_val(max_packets);

  caml_release_runtime_system();

  if (MOSQ_ERR_SUCCESS != (rc = mosquitto_loop_forever(mosq->conn, timeout_, max_packets_))) {
    lerrno = errno;
  }

  caml_acquire_runtime_system();

  if (MOSQ_ERR_SUCCESS == rc) {
    result = RESULT_OK;
    Store_field(result, 0, Val_unit);
  } else {
    switch (rc) {
      case MOSQ_ERR_INVAL: lerrno = EINVAL; break;
      case MOSQ_ERR_NOMEM: lerrno = ENOMEM; break;
      case MOSQ_ERR_NO_CONN: lerrno = ENOTCONN; break;
      case MOSQ_ERR_CONN_LOST: lerrno =  ENOTCONN; break;
      case MOSQ_ERR_PROTOCOL: lerrno = EPROTOTYPE; break;
      case MOSQ_ERR_ERRNO: break;
      default: assert(false);
    }
    perrno = caml_alloc(2, 0);
    Store_field(perrno, 0, eunix); // `EUnix
    Store_field(perrno, 1, unix_error_of_code(lerrno));

    result = RESULT_ERROR;
    Store_field(result, 0, perrno);
  }

  CAMLreturn(result);
}

