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

typedef enum callback {
  CBCONNECT = 0,
  CBDISCONNECT = 1,
  CBPUBLISH = 2,
  CBMESSAGE = 3,
  CBSUBSCRIBE = 4,
  CBUNSUBSCRIBE = 5,
  CBLOG = 6
} callback;

struct ocmq {
  struct mosquitto *conn;
  value *cb[7];
  char uid[7][32];
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

  struct ocmq *mq;
  int lerrno;
  char *id_;
  bool clean_session_;
  size_t id_len;

  id_len = caml_string_length(id);
  id_ = calloc(1, 1 + id_len);
  memcpy(id_, String_val(id), id_len);
  id_[id_len] = '\0';

  clean_session_ = Bool_val(clean_session);

  caml_release_runtime_system();
  mq = calloc(1, sizeof(struct ocmq));
  snprintf(mq->uid[CBCONNECT], 32, "%p_connect", (void*)mq);
  snprintf(mq->uid[CBDISCONNECT], 32, "%p_disconnect", (void*)mq);
  snprintf(mq->uid[CBPUBLISH], 32, "%p_publish", (void*)mq);
  snprintf(mq->uid[CBMESSAGE], 32, "%p_message", (void*)mq);
  snprintf(mq->uid[CBSUBSCRIBE], 32, "%p_subscribe", (void*)mq);
  snprintf(mq->uid[CBUNSUBSCRIBE], 32, "%p_unsubscribe", (void*)mq);
  snprintf(mq->uid[CBLOG], 32, "%p_log", (void*)mq);

  if (NULL == (mq->conn = mosquitto_new(id_, clean_session_, mq))) {
    lerrno = errno;
  }
  free(id_);

  caml_acquire_runtime_system();

  if (NULL != mq) {
    result = RESULT_OK;
    Store_field(result, 0, (value)mq);
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

  struct ocmq *mq;
  char *host_;
  size_t len;
  int lerrno, port_, keepalive_, rc;
  len = caml_string_length(host);
  host_ = calloc(1, 1 + len);
  memcpy(host_, String_val(host), len);
  host_[len] = '\0';

  port_ = Long_val(port);
  keepalive_ = Long_val(keepalive);

  mq = (struct ocmq*)mqtt;

  caml_release_runtime_system();

  if (MOSQ_ERR_SUCCESS != (rc = mosquitto_connect(mq->conn, host_, port_, keepalive_))) {
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

  struct ocmq *mq;
  int lerrno, rc;

  mq = (struct ocmq*)mqtt;

  caml_release_runtime_system();

  if (MOSQ_ERR_SUCCESS != (rc = mosquitto_reconnect(mq->conn))) {
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
  struct ocmq *mq;
  int qos_, rc, lerrno;
  bool retain_;

  qos_ = Long_val(qos);
  retain_ = Bool_val(retain);

  mq = (struct ocmq*)mqtt;

  topic_len = caml_string_length(topic);
  topic_ = calloc(1, topic_len + 1);
  memcpy(topic_, String_val(topic), topic_len);
  topic_[topic_len] = '\0';

  payload_len = caml_string_length(payload);
  payload_ = calloc(1, payload_len);
  memcpy(payload_, String_val(payload), payload_len);

  caml_release_runtime_system();

  rc = mosquitto_publish(mq->conn, NULL, topic_, payload_len, payload_, qos_, retain_);
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
  struct ocmq *mq;
  int rc, lerrno, qos_;

  qos_ = Long_val(qos);
  mq = (struct ocmq*)mqtt;

  topic_len = caml_string_length(topic);
  topic_ = calloc(1, topic_len + 1);
  memcpy(topic_, String_val(topic), topic_len);
  topic_[topic_len] = '\0';

  caml_release_runtime_system();

  rc = mosquitto_subscribe(mq->conn, NULL, topic_, qos_);
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

void mqtt_callback_msg(struct mosquitto *m, void *obj, const struct mosquitto_message *msg_) {
  static value * f = NULL;
  struct ocmq *mq;
  size_t topic_len;
  value msg, payload;

  mq = (struct ocmq*)obj;

  caml_acquire_runtime_system();

  if (NULL == f) {
    f = caml_named_value(mq->uid[CBMESSAGE]);
  }
  if (NULL != f) {
    msg = caml_alloc_tuple(5);

    payload = caml_alloc_string(msg_->payloadlen);
    memcpy(String_val(payload), msg_->payload, msg_->payloadlen);

    topic_len = strlen(msg_->topic);
    value topic = caml_alloc_string(topic_len);
    memcpy(String_val(topic), msg_->topic, topic_len);

    Store_field(msg, 0, Val_int(msg_->mid));
    Store_field(msg, 1, topic);
    Store_field(msg, 2, payload);
    Store_field(msg, 3, Val_int(msg_->qos));
    Store_field(msg, 4, Val_bool(msg_->retain));

    caml_callback(*f, msg);
  }

  caml_release_runtime_system();
}

void mqtt_callback_con(struct mosquitto *m, void *obj, int rc) {
  struct ocmq *mq;
  mq = (struct ocmq*)obj;

  caml_acquire_runtime_system();

  if (NULL == mq->cb[CBCONNECT]) {
    mq->cb[CBCONNECT] = caml_named_value(mq->uid[CBCONNECT]);
  }
  if (NULL != mq->cb[CBCONNECT]) {
    caml_callback(*(mq->cb[CBCONNECT]), Long_val(rc));
  }

  caml_release_runtime_system();
}

void mqtt_callback_dco(struct mosquitto *m, void *obj, int rc) {
  struct ocmq *mq;
  mq = (struct ocmq*)obj;

  caml_acquire_runtime_system();

  if (NULL == mq->cb[CBDISCONNECT]) {
    mq->cb[CBDISCONNECT] = caml_named_value(mq->uid[CBDISCONNECT]);
  }
  if (NULL != mq->cb[CBDISCONNECT]) {
    caml_callback(*(mq->cb[CBDISCONNECT]), Long_val(rc));
  }

  caml_release_runtime_system();
}

void mqtt_callback_sub(struct mosquitto *m, void *obj, int mid, int qos_count, const int *qos) {
  struct ocmq *mq;
  value qos_;
  mq = (struct ocmq*)obj;

  caml_acquire_runtime_system();

  if (NULL == mq->cb[CBSUBSCRIBE]) {
    mq->cb[CBSUBSCRIBE] = caml_named_value(mq->uid[CBSUBSCRIBE]);
  }
  if (NULL != mq->cb[CBSUBSCRIBE]) {
    // TODO: alloc list of qos, fix call!
    // qos_ = ..
    caml_callback(*(mq->cb[CBSUBSCRIBE]), Val_unit);
  }

  caml_release_runtime_system();
}

void mqtt_callback_usu(struct mosquitto *m, void *obj, int rc) {
  struct ocmq *mq;
  mq = (struct ocmq*)obj;

  caml_acquire_runtime_system();

  if (NULL == mq->cb[CBUNSUBSCRIBE]) {
    mq->cb[CBUNSUBSCRIBE] = caml_named_value(mq->uid[CBUNSUBSCRIBE]);
  }
  if (NULL != mq->cb[CBUNSUBSCRIBE]) {
    caml_callback(*(mq->cb[CBUNSUBSCRIBE]), Long_val(rc));
  }

  caml_release_runtime_system();
}

void mqtt_callback_pub(struct mosquitto *m, void *obj, int rc) {
  struct ocmq *mq;
  mq = (struct ocmq*)obj;

  caml_acquire_runtime_system();

  if (NULL == mq->cb[CBPUBLISH]) {
    mq->cb[CBPUBLISH] = caml_named_value(mq->uid[CBPUBLISH]);
  }
  if (NULL != mq->cb[CBPUBLISH]) {
    caml_callback(*(mq->cb[CBPUBLISH]), Long_val(rc));
  }

  caml_release_runtime_system();
}

CAMLprim value mqtt_connect_callback_set(value mqtt) {
  CAMLparam1(mqtt);
  CAMLlocal1(uid);
  size_t len;
  struct ocmq *mq;

  mq = (struct ocmq*)mqtt;

  caml_release_runtime_system();

  mosquitto_connect_callback_set(mq->conn, mqtt_callback_con);

  caml_acquire_runtime_system();

  len = strlen(mq->uid[CBCONNECT]);
  uid = caml_alloc_string(len);
  memcpy(String_val(uid), (void*)mq->uid[CBCONNECT], len);

  CAMLreturn(uid);
}

CAMLprim value mqtt_disconnect_callback_set(value mqtt) {
  CAMLparam1(mqtt);
  CAMLlocal1(uid);
  size_t len;
  struct ocmq *mq;

  mq = (struct ocmq*)mqtt;

  caml_release_runtime_system();

  mosquitto_connect_callback_set(mq->conn, mqtt_callback_dco);

  caml_acquire_runtime_system();

  len = strlen(mq->uid[CBDISCONNECT]);
  uid = caml_alloc_string(len);
  memcpy(String_val(uid), (void*)mq->uid[CBDISCONNECT], len);

  CAMLreturn(uid);
}

CAMLprim value mqtt_subscribe_callback_set(value mqtt) {
  CAMLparam1(mqtt);
  CAMLlocal1(uid);
  size_t len;
  struct ocmq *mq;

  mq = (struct ocmq*)mqtt;

  caml_release_runtime_system();

  mosquitto_connect_callback_set(mq->conn, mqtt_callback_con);

  caml_acquire_runtime_system();

  len = strlen(mq->uid[CBSUBSCRIBE]);
  uid = caml_alloc_string(len);
  memcpy(String_val(uid), (void*)mq->uid[CBSUBSCRIBE], len);

  CAMLreturn(uid);
}

CAMLprim value mqtt_unsubscribe_callback_set(value mqtt) {
  CAMLparam1(mqtt);
  CAMLlocal1(uid);
  size_t len;
  struct ocmq *mq;

  mq = (struct ocmq*)mqtt;

  caml_release_runtime_system();

  mosquitto_connect_callback_set(mq->conn, mqtt_callback_usu);

  caml_acquire_runtime_system();

  len = strlen(mq->uid[CBUNSUBSCRIBE]);
  uid = caml_alloc_string(len);
  memcpy(String_val(uid), (void*)mq->uid[CBUNSUBSCRIBE], len);

  CAMLreturn(uid);
}

CAMLprim value mqtt_publish_callback_set(value mqtt) {
  CAMLparam1(mqtt);
  CAMLlocal1(uid);
  size_t len;
  struct ocmq *mq;

  mq = (struct ocmq*)mqtt;

  caml_release_runtime_system();

  mosquitto_publish_callback_set(mq->conn, mqtt_callback_pub);

  caml_acquire_runtime_system();

  len = strlen(mq->uid[CBPUBLISH]);
  uid = caml_alloc_string(len);
  memcpy(String_val(uid), (void*)mq->uid[CBPUBLISH], len);

  CAMLreturn(uid);
}

CAMLprim value mqtt_message_callback_set(value mqtt) {
  CAMLparam1(mqtt);
  CAMLlocal1(uid);
  size_t len;
  struct ocmq *mq;

  mq = (struct ocmq*)mqtt;

  caml_release_runtime_system();

  mosquitto_message_callback_set(mq->conn, mqtt_callback_msg);

  caml_acquire_runtime_system();

  len = strlen(mq->uid[CBMESSAGE]);
  uid = caml_alloc_string(len);
  memcpy(String_val(uid), (void*)mq->uid[CBMESSAGE], len);

  CAMLreturn(uid);
}

CAMLprim value mqtt_loop(value mqtt, value timeout, value max_packets) {
  CAMLparam3(mqtt, timeout, max_packets);
  CAMLlocal2(perrno, result);
  struct ocmq *mq;
  int timeout_, max_packets_, lerrno, rc;

  mq = (struct ocmq*)mqtt;
  timeout_ = Long_val(timeout);

  max_packets_ = Long_val(max_packets);

  caml_release_runtime_system();

  if (MOSQ_ERR_SUCCESS != (rc = mosquitto_loop(mq->conn, timeout_, max_packets_))) {
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
  struct ocmq *mq;
  int timeout_, max_packets_, lerrno, rc;

  mq = (struct ocmq*)mqtt;
  timeout_ = Long_val(timeout);

  max_packets_ = Long_val(max_packets);

  caml_release_runtime_system();

  if (MOSQ_ERR_SUCCESS != (rc = mosquitto_loop_forever(mq->conn, timeout_, max_packets_))) {
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

CAMLprim value mqtt_socket(value mqtt) {
  CAMLparam1(mqtt);
  int fd;
  struct ocmq *mq;

  mq = (struct ocmq*)mqtt;
  fd = mosquitto_socket(mq->conn);

  CAMLreturn(Val_long(fd));
}

