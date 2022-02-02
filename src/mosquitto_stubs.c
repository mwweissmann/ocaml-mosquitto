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
#include <caml/custom.h>
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
    long id;
};

#define OCMQ(val) ((struct ocmq **) Data_custom_val(val))

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

static value wrap_result(int rc, value retval, int lerrno) {
  CAMLparam1(retval);
  CAMLlocal2(result, perrno);
  if (MOSQ_ERR_SUCCESS == rc) {
    result = RESULT_OK;
    Store_field(result, 0, retval);
  } else {
    switch (rc) {
      case MOSQ_ERR_INVAL: lerrno = EINVAL; break;
      case MOSQ_ERR_NOMEM: lerrno = ENOMEM; break;
      case MOSQ_ERR_NO_CONN: lerrno = ENOTCONN; break;
      case MOSQ_ERR_CONN_LOST: lerrno =  ENOTCONN; break;
      case MOSQ_ERR_PROTOCOL: lerrno = EPROTOTYPE; break;
      case MOSQ_ERR_PAYLOAD_SIZE: lerrno = EMSGSIZE; break;
      case MOSQ_ERR_ERRNO: break;
      default: lerrno = EPERM; break;
    }
    perrno = caml_alloc(2, 0);
    Store_field(perrno, 0, eunix); // `EUnix
    Store_field(perrno, 1, unix_error_of_code(lerrno));

    result = RESULT_ERROR;
    Store_field(result, 0, perrno);
  }
  CAMLreturn(result);
}

static struct custom_operations mq_ops = {
  .identifier = "Mosquitto.mqtt",
  .finalize = NULL,
  .compare = NULL,
  .hash = NULL,
  .serialize = NULL,
  .deserialize = NULL,
  .compare_ext = NULL,
  .fixed_length = NULL
};

static value extract_message_tuple(const struct mosquitto_message *msg_) {
  CAMLparam0();
  CAMLlocal3(msg, payload, topic);

  size_t topic_len;

  msg = caml_alloc_tuple(5);

  payload = caml_alloc_initialized_string(msg_->payloadlen, msg_->payload);

  topic_len = strlen(msg_->topic);
  topic = caml_alloc_initialized_string(topic_len, msg_->topic);

  Store_field(msg, 0, Val_int(msg_->mid));
  Store_field(msg, 1, topic);
  Store_field(msg, 2, payload);
  Store_field(msg, 3, Val_int(msg_->qos));
  Store_field(msg, 4, Val_bool(msg_->retain));

  CAMLreturn(msg);
}

static void mqtt_callback_msg(struct mosquitto *m, void *obj, const struct mosquitto_message *msg_) {
  struct ocmq *mq;
  value msg;
  static const value *cb = NULL;

  mq = (struct ocmq*)obj;
  if (mq->conn == NULL) return;

  caml_acquire_runtime_system();

  if (cb == NULL)
      cb = caml_named_value("Mosquitto.message");

  if (cb != NULL) {
    msg = extract_message_tuple(msg_);
    caml_callback2(*cb, Val_long(mq->id), msg);
  }

  caml_release_runtime_system();
}

static void mqtt_callback_log(struct mosquitto *m, void *obj, int lvl, const char *str) {
  struct ocmq *mq;
  value str_;
  size_t len;
  static const value *cb = NULL;

  mq = (struct ocmq*)obj;
  if (mq->conn == NULL) return;

  caml_acquire_runtime_system();

  if (cb == NULL)
      cb = caml_named_value("Mosquitto.log");

  if (cb != NULL) {
    len = strlen(str);
    str_ = caml_alloc_initialized_string(len, str);
    caml_callback3(*cb, Val_long(mq->id), Val_long(lvl), str_);
  }

  caml_release_runtime_system();
}

static void mqtt_callback_con(struct mosquitto *m, void *obj, int rc) {
  struct ocmq *mq;
  static const value *cb = NULL;

  mq = (struct ocmq*)obj;
  if (mq->conn == NULL) return;

  caml_acquire_runtime_system();

  if (cb == NULL)
      cb = caml_named_value("Mosquitto.connect");

  if (cb != NULL) {
    caml_callback2(*cb, Val_long(mq->id), Val_long(rc));
  }

  caml_release_runtime_system();
}

static void mqtt_callback_dco(struct mosquitto *m, void *obj, int rc) {
  struct ocmq *mq;
  static const value *cb = NULL;

  mq = (struct ocmq*)obj;
  if (mq->conn == NULL) return;

  caml_acquire_runtime_system();

  if (cb == NULL)
      cb = caml_named_value("Mosquitto.disconnect");

  if (cb != NULL) {
    caml_callback2(*cb, Val_long(mq->id), Val_long(rc));
  }

  caml_release_runtime_system();
}

static value extract_qos_list(int qos_count, const int *qos) {
  CAMLparam0();
  CAMLlocal2(cli, cons);

  int i;

  cli = Val_emptylist;
  for (i = qos_count; i > 0; i--) {
    cons = caml_alloc(2, 0);
    Store_field(cons, 0, Val_long(qos[i - 1]));
    Store_field(cons, 1, cli);
    cli = cons;
  }

  CAMLreturn(cli);
}

static void mqtt_callback_sub(struct mosquitto *m, void *obj, int mid, int qos_count, const int *qos) {
  struct ocmq *mq;
  value cli;
  static const value *cb = NULL;

  mq = (struct ocmq*)obj;
  if (mq->conn == NULL) return;

  caml_acquire_runtime_system();

  if (cb == NULL)
      cb = caml_named_value("Mosquitto.subscribe");

  if (cb != NULL) {
      cli = extract_qos_list(qos_count, qos);
      caml_callback3(*cb, Val_long(mq->id), Val_long(mid), cli);
  }

  caml_release_runtime_system();
}

static void mqtt_callback_usu(struct mosquitto *m, void *obj, int rc) {
  struct ocmq *mq;
  static const value *cb = NULL;

  mq = (struct ocmq*)obj;
  if (mq->conn == NULL) return;

  caml_acquire_runtime_system();

  if (cb == NULL)
      cb = caml_named_value("Mosquitto.unsubscribe");

  if (cb != NULL) {
    caml_callback2(*cb, Val_long(mq->id), Val_long(rc));
  }

  caml_release_runtime_system();
}

static void mqtt_callback_pub(struct mosquitto *m, void *obj, int rc) {
  struct ocmq *mq;
  static const value *cb = NULL;

  mq = (struct ocmq*)obj;
  if (mq->conn == NULL) return;

  caml_acquire_runtime_system();

  if (cb == NULL)
      cb = caml_named_value("Mosquitto.publish");

  if (cb != NULL) {
    caml_callback2(*cb, Val_long(mq->id), Val_long(rc));
  }

  caml_release_runtime_system();
}

CAMLprim value mqtt_create(value client_id, value clean_session, value id) {
    CAMLparam3(client_id, clean_session, id);
    CAMLlocal3(result, mqcustom, mqtt);

    struct ocmq *mq;
    char *client_id_;
    bool clean_session_;
    size_t client_id_len;

    client_id_len = caml_string_length(client_id);
    client_id_ = calloc(1, 1 + client_id_len);
    if (client_id_ == NULL) {
        result = wrap_result(MOSQ_ERR_NOMEM, Val_unit, 0);
        goto exit;
    }
    memcpy(client_id_, String_val(client_id), client_id_len);
    client_id_[client_id_len] = '\0';

    clean_session_ = Bool_val(clean_session);

    caml_release_runtime_system();
    mq = calloc(1, sizeof(struct ocmq));
    if (mq != NULL) {
        mq->id = Long_val(id);
        mq->conn = mosquitto_new(client_id_, clean_session_, mq);
        if (mq->conn != NULL) {
            mosquitto_message_callback_set(mq->conn, mqtt_callback_msg);
            mosquitto_connect_callback_set(mq->conn, mqtt_callback_con);
            mosquitto_connect_callback_set(mq->conn, mqtt_callback_dco);
            mosquitto_subscribe_callback_set(mq->conn, mqtt_callback_sub);
            mosquitto_unsubscribe_callback_set(mq->conn, mqtt_callback_usu);
            mosquitto_publish_callback_set(mq->conn, mqtt_callback_pub);
            mosquitto_log_callback_set(mq->conn, mqtt_callback_log);
        }
    };
    int lerrno = errno;
    caml_acquire_runtime_system();
    free(client_id_);

    if (mq == NULL) {
        result = wrap_result(MOSQ_ERR_NOMEM, Val_unit, 0);
        goto exit;
    };

    if (mq->conn != NULL) {
        mqcustom = caml_alloc_custom(&mq_ops, sizeof(struct ocmq *), 0, 1);
        *(OCMQ(mqcustom)) = mq;
        result = wrap_result(MOSQ_ERR_SUCCESS, mqcustom, lerrno);
    } else {
        result = wrap_result(MOSQ_ERR_ERRNO, Val_unit, lerrno);
    }

exit:
  CAMLreturn(result);
}

CAMLprim value mqtt_destroy(value mqtt) {
  CAMLparam1(mqtt);
  CAMLlocal1(result);

  struct ocmq *mq;

  mq = *(OCMQ(mqtt));
  if (mq->conn != NULL)
      mosquitto_destroy(mq->conn);
  mq->conn = NULL;

  CAMLreturn(result);
}

CAMLprim value mqtt_set_basic_auth(value mqtt, value username, value password) {
    CAMLparam3(mqtt, username, password);
    CAMLlocal1(result);

    struct ocmq *mq;
    mq = *(OCMQ(mqtt));
    if (mq->conn == NULL) caml_invalid_argument("Mosquitto: already destroyed");
    int rc = mosquitto_username_pw_set(mq->conn, String_val(username), String_val(password));
    result = wrap_result(rc, Val_unit, errno);

    CAMLreturn(result);
}

CAMLprim value mqtt_connect(value mqtt, value host, value port, value keepalive) {
  CAMLparam4(mqtt, host, port, keepalive);
  CAMLlocal1(result);

  struct ocmq *mq;
  char *host_ = NULL;
  size_t len;
  int port_, keepalive_, rc;
  len = caml_string_length(host);
  host_ = calloc(1, 1 + len);
  if (host_ == NULL) {
      result = wrap_result(MOSQ_ERR_NOMEM, Val_unit, 0);
      goto exit;
  };
  memcpy(host_, String_val(host), len);
  host_[len] = '\0';

  port_ = Long_val(port);
  keepalive_ = Long_val(keepalive);

  mq = *(OCMQ(mqtt));
  if (mq->conn == NULL) caml_invalid_argument("Mosquitto: already destroyed");
  caml_release_runtime_system();
  rc = mosquitto_connect(mq->conn, host_, port_, keepalive_);
  int lerrno = errno;
  caml_acquire_runtime_system();
  result = wrap_result(rc, Val_unit, lerrno);

exit:
  free(host_);
  CAMLreturn(result);
}

CAMLprim value mqtt_reconnect(value mqtt) {
  CAMLparam1(mqtt);
  CAMLlocal1(result);

  struct ocmq *mq;
  int rc;

  mq = *(OCMQ(mqtt));
  if (mq->conn == NULL) caml_invalid_argument("Mosquitto: already destroyed");

  caml_release_runtime_system();
  rc = mosquitto_reconnect(mq->conn);
  caml_acquire_runtime_system();
  int lerrno = errno;
  result = wrap_result(rc, Val_unit, lerrno);

  CAMLreturn(result);
}

CAMLprim value mqtt_disconnect(value mqtt) {
  CAMLparam1(mqtt);
  CAMLlocal1(result);

  struct ocmq *mq;
  int rc;

  mq = *(OCMQ(mqtt));
  if (mq->conn == NULL) caml_invalid_argument("Mosquitto: already destroyed");

  caml_release_runtime_system();
  rc = mosquitto_disconnect(mq->conn);
  int lerrno = errno;
  caml_acquire_runtime_system();
  result = wrap_result(rc, Val_unit, lerrno);

  CAMLreturn(result);
}

CAMLprim value mqtt_publish(value mqtt, value msg) {
  CAMLparam2(mqtt, msg);
  CAMLlocal1(result);

  char *topic = NULL;
  char *payload = NULL;
  size_t topic_len, payload_len;
  struct ocmq *mq;
  int qos, rc, mid, *mid_pt;
  bool retain;

  qos = Long_val(Field(msg, 3));
  retain = Bool_val(Field(msg, 4));
  mid = Long_val(Field(msg, 0));
  mid_pt = ((mid & 0xFFFF) == mid) ? &mid : NULL;

  mq = *(OCMQ(mqtt));
  if (mq->conn == NULL) caml_invalid_argument("Mosquitto: already destroyed");

  topic_len = caml_string_length(Field(msg, 1));
  topic = calloc(1, topic_len + 1);
  if (topic == NULL) {
      result = wrap_result(MOSQ_ERR_NOMEM, Val_unit, 0);
      goto exit;
  };
  memcpy(topic, String_val(Field(msg, 1)), topic_len);
  topic[topic_len] = '\0';

  payload_len = caml_string_length(Field(msg, 2));
  payload = calloc(1, payload_len);
  if (payload == NULL) {
      free(topic);
      result = wrap_result(MOSQ_ERR_NOMEM, Val_unit, 0);
      goto exit;
  };
  memcpy(payload, String_val(Field(msg, 2)), payload_len);

  caml_release_runtime_system();
  rc = mosquitto_publish(mq->conn, mid_pt, topic, payload_len, payload, qos, retain);
  int lerrno = errno;
  caml_acquire_runtime_system();
  result = wrap_result(rc, Val_unit, lerrno);

exit:
  free(topic);
  free(payload);
  CAMLreturn(result);
}

CAMLprim value mqtt_subscribe(value mqtt, value topic, value qos) {
  CAMLparam3(mqtt, topic, qos);
  CAMLlocal1(result);

  char *topic_ = NULL;
  size_t topic_len;
  struct ocmq *mq;
  int rc, qos_;

  qos_ = Long_val(qos);
  mq = *(OCMQ(mqtt));
  if (mq->conn == NULL) caml_invalid_argument("Mosquitto: already destroyed");

  topic_len = caml_string_length(topic);
  topic_ = calloc(1, topic_len + 1);
  if (topic_ == NULL) {
      result = wrap_result(MOSQ_ERR_NOMEM, Val_unit, 0);
      goto exit;
  };
  memcpy(topic_, String_val(topic), topic_len);
  topic_[topic_len] = '\0';

  caml_release_runtime_system();
  rc = mosquitto_subscribe(mq->conn, NULL, topic_, qos_);
  int lerrno = errno;
  caml_acquire_runtime_system();
  result = wrap_result(rc, Val_unit, lerrno);

exit:
  free(topic_);
  CAMLreturn(result);
}

CAMLprim value mqtt_loop(value mqtt, value timeout, value max_packets) {
  CAMLparam3(mqtt, timeout, max_packets);
  CAMLlocal1(result);
  struct ocmq *mq;
  int timeout_, max_packets_, rc;

  mq = *(OCMQ(mqtt));
  if (mq->conn == NULL) caml_invalid_argument("Mosquitto: already destroyed");
  timeout_ = Long_val(timeout);

  max_packets_ = Long_val(max_packets);

  caml_release_runtime_system();
  rc = mosquitto_loop(mq->conn, timeout_, max_packets_);
  int lerrno = errno;
  caml_acquire_runtime_system();
  result = wrap_result(rc, Val_unit, lerrno);

  CAMLreturn(result);
}

CAMLprim value mqtt_loop_forever(value mqtt, value timeout, value max_packets) {
  CAMLparam3(mqtt, timeout, max_packets);
  CAMLlocal1(result);
  struct ocmq *mq;
  int timeout_, max_packets_, rc;

  mq = *(OCMQ(mqtt));
  if (mq->conn == NULL) caml_invalid_argument("Mosquitto: already destroyed");
  timeout_ = Long_val(timeout);

  max_packets_ = Long_val(max_packets);

  caml_release_runtime_system();
  rc = mosquitto_loop_forever(mq->conn, timeout_, max_packets_);
  int lerrno = errno;
  caml_acquire_runtime_system();
  result = wrap_result(rc, Val_unit, lerrno);

  CAMLreturn(result);
}

CAMLprim value mqtt_socket(value mqtt) {
  CAMLparam1(mqtt);
  int fd;
  struct ocmq *mq;

  mq = *(OCMQ(mqtt));
  if (mq->conn == NULL) caml_invalid_argument("Mosquitto: already destroyed");
  fd = mosquitto_socket(mq->conn);

  CAMLreturn(Val_long(fd));
}

CAMLprim value mqtt_loop_read(value mqtt, value max_packets) {
  CAMLparam2(mqtt, max_packets);
  CAMLlocal1(result);
  struct ocmq *mq;
  int max_packets_, rc;

  mq = *(OCMQ(mqtt));
  if (mq->conn == NULL) caml_invalid_argument("Mosquitto: already destroyed");

  max_packets_ = Long_val(max_packets);

  caml_release_runtime_system();
  rc = mosquitto_loop_read(mq->conn, max_packets_);
  int lerrno = errno;
  caml_acquire_runtime_system();
  result = wrap_result(rc, Val_unit, lerrno);

  CAMLreturn(result);
}

CAMLprim value mqtt_loop_write(value mqtt, value max_packets) {
  CAMLparam2(mqtt, max_packets);
  CAMLlocal1(result);
  struct ocmq *mq;
  int max_packets_, rc;

  mq = *(OCMQ(mqtt));
  if (mq->conn == NULL) caml_invalid_argument("Mosquitto: already destroyed");

  max_packets_ = Long_val(max_packets);

  caml_release_runtime_system();
  rc = mosquitto_loop_write(mq->conn, max_packets_);
  int lerrno = errno;
  caml_acquire_runtime_system();
  result = wrap_result(rc, Val_unit, lerrno);

  CAMLreturn(result);
}

CAMLprim value mqtt_loop_misc(value mqtt) {
  CAMLparam1(mqtt);
  CAMLlocal1(result);
  struct ocmq *mq;
  int rc;

  mq = *(OCMQ(mqtt));
  if (mq->conn == NULL) caml_invalid_argument("Mosquitto: already destroyed");

  caml_release_runtime_system();
  rc = mosquitto_loop_misc(mq->conn);
  int lerrno = errno;
  caml_acquire_runtime_system();
  result = wrap_result(rc, Val_unit, lerrno);

  CAMLreturn(result);
}
