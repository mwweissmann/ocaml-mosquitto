type t

module Message = struct
  type t = {
    mid : int;
    topic : string;
    payload : string;
    qos : int;
    retain : bool;
  }

  let create ?mid:(m=(-1)) ~topic ?qos:(q=0) ?retain:(r=false) payload =
    let mid = if ((m land 0xFFFF) = m) then m else -1 in
    { mid; topic; payload; qos = q; retain = r }

  let mid x = if (x.mid land 0xFFFF) = x.mid then Some x.mid else None

  let topic x = x.topic

  let payload x = x.payload

  let qos x = x.qos

  let retain x = x.retain
end

external initialize : unit -> (int * int * int * int) = "mqtt_initialize"

module Version = struct
  let version, major, minor, revision = initialize ()
end

external create : string -> bool -> (t, [>`EUnix of Unix.error]) Result.result = "mqtt_create"

external destroy : t -> unit = "mqtt_destroy"

external set_basic_auth : t -> string -> string -> (unit, [>`EUnix of Unix.error]) Result.result = "mqtt_set_basic_auth"

external connect : t -> string -> int -> int -> (unit, [>`EUnix of Unix.error]) Result.result = "mqtt_connect"

external disconnect : t -> (unit, [>`EUnix of Unix.error]) Result.result = "mqtt_disconnect"

external reconnect : t -> (unit, [>`EUnix of Unix.error]) Result.result = "mqtt_reconnect"

external publish : t -> Message.t -> (unit, [>`EUnix of Unix.error]) Result.result = "mqtt_publish"

external subscribe : t -> string -> int -> (unit, [>`EUnix of Unix.error]) Result.result = "mqtt_subscribe"

external mosquitto_message_callback_set : t -> string = "mqtt_message_callback_set"
let set_callback_message m = Callback.register (mosquitto_message_callback_set m)

external mosquitto_subscribe_callback_set : t -> string = "mqtt_subscribe_callback_set"
let set_callback_subscribe m = Callback.register (mosquitto_subscribe_callback_set m)

external mosquitto_unsubscribe_callback_set : t -> string = "mqtt_unsubscribe_callback_set"
let set_callback_unsubscribe m = Callback.register (mosquitto_unsubscribe_callback_set m)

external mosquitto_publish_callback_set : t -> string = "mqtt_publish_callback_set"
let set_callback_publish m = Callback.register (mosquitto_publish_callback_set m)

external mosquitto_message_callback_set : t -> string = "mqtt_message_callback_set"
let set_callback_message m = Callback.register (mosquitto_message_callback_set m)

external mosquitto_connect_callback_set : t -> string = "mqtt_connect_callback_set"
let set_callback_connect m = Callback.register (mosquitto_connect_callback_set m)

external mosquitto_disconnect_callback_set : t -> string = "mqtt_disconnect_callback_set"
let set_callback_disconnect m = Callback.register (mosquitto_disconnect_callback_set m)

external mosquitto_log_callback_set : t -> string = "mqtt_log_callback_set"
let set_callback_log m = Callback.register (mosquitto_log_callback_set m)

external loop : t -> int -> int -> (unit, [>`EUnix of Unix.error]) Result.result = "mqtt_loop"

external loop_forever : t -> int -> int -> (unit, [>`EUnix of Unix.error]) Result.result = "mqtt_loop_forever"

external socket : t -> Unix.file_descr = "mqtt_socket"

external loop_read : t -> int -> (unit, [>`EUnix of Unix.error]) Result.result = "mqtt_loop_read"

external loop_write : t -> int -> (unit, [>`EUnix of Unix.error]) Result.result = "mqtt_loop_write"

external loop_misc : t -> (unit, [>`EUnix of Unix.error]) Result.result = "mqtt_loop_misc"
