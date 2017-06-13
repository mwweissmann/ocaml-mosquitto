type m
type t = m * string

type msg = {
  mid : int;
  topic : string;
  payload : string;
  qos : int;
  retain : bool;
}

external create : string -> bool -> (m * string, [>`EUnix of Unix.error]) Result.result = "mqtt_create"

external mosquitto_connect : m -> string -> int -> int -> (unit, [>`EUnix of Unix.error]) Result.result = "mqtt_connect"
let connect (m, _) = mosquitto_connect m

external mosquitto_reconnect : m -> (unit, [>`EUnix of Unix.error]) Result.result = "mqtt_reconnect"
let reconnect (m, _) = mosquitto_reconnect m

external mosquitto_publish : m -> string -> string -> int -> bool -> (unit, [>`EUnix of Unix.error]) Result.result = "mqtt_publish"
let publish (m, _) = mosquitto_publish m

external mosquitto_subscribe : m -> string -> int -> (unit, [>`EUnix of Unix.error]) Result.result = "mqtt_subscribe"
let subscribe (m, _) = mosquitto_subscribe m

external mosquitto_callback_set : m -> unit = "mqtt_message_callback_set"
let callback_set (m, uid) f =
  let () = Callback.register uid f in
  mosquitto_callback_set m

external mosquitto_loop : m -> int -> int -> (unit, [>`EUnix of Unix.error]) Result.result = "mqtt_loop"
let loop (m, _) = mosquitto_loop m

external mosquitto_initialize : unit -> unit = "mqtt_initialize"
let () = mosquitto_initialize ()


