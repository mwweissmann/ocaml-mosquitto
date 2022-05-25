type mq

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

type wrapper =
  { mq: mq;
    id: int;
    mutable cb_message: Message.t -> unit;
    mutable cb_connect: int -> unit;
    mutable cb_disconnect: int -> unit;
    mutable cb_publish: int -> unit;
    mutable cb_subscribe: int -> int list -> unit;
    mutable cb_unsubscribe: int -> unit;
    mutable cb_log: int -> string -> unit;
  }

let empty =
  { mq = Obj.magic ();
    id = 0;
    cb_message = (fun _ -> ());
    cb_connect = (fun _ -> ());
    cb_disconnect = (fun _ -> ());
    cb_publish = (fun _ -> ());
    cb_subscribe = (fun _ _ -> ());
    cb_unsubscribe = (fun _ -> ());
    cb_log = (fun _ _ -> ());
  }

module T = struct
  type t = wrapper
  let equal x y = x.id = y.id
  let hash x = x.id
end

module W = Weak.Make(T)

type t = wrapper

let next_id = ref 0
let table = W.create 13

let wrap id mq =
  let w = { empty with mq; id } in
  W.add table w;
  w

let find id =
  let w = { empty with id } in
  try
    W.find table w
  with
    | Not_found -> empty

let dispatch_message id msg =
  (find id).cb_message msg

let dispatch_connect id rc =
  (find id).cb_connect rc

let dispatch_disconnect id rc =
  (find id).cb_disconnect rc

let dispatch_publish id rc =
  (find id).cb_publish rc

let dispatch_subscribe id mid qos_list =
  (find id).cb_subscribe mid qos_list

let dispatch_unsubscribe id rc =
  (find id).cb_unsubscribe rc

let dispatch_log id level str =
  (find id).cb_log level str

external initialize : unit -> (int * int * int * int) = "mqtt_initialize"

module Version = struct
  let () =
    Callback.register "Mosquitto.message" dispatch_message;
    Callback.register "Mosquitto.connect" dispatch_connect;
    Callback.register "Mosquitto.disconnect" dispatch_disconnect;
    Callback.register "Mosquitto.publish" dispatch_publish;
    Callback.register "Mosquitto.subscribe" dispatch_subscribe;
    Callback.register "Mosquitto.unsubscribe" dispatch_unsubscribe;
    Callback.register "Mosquitto.log" dispatch_log
  let version, major, minor, revision = initialize ()
end

external mqtt_destroy : mq -> unit = "mqtt_destroy"

let destroy w = mqtt_destroy w.mq

external mqtt_create : string -> bool -> int -> (mq, [>`EUnix of Unix.error]) result = "mqtt_create"

let create client_id clean_session =
  let id = !next_id in
  incr next_id;
  mqtt_create client_id clean_session id
  |> Result.map (wrap id)
  |> Result.map (fun w -> Gc.finalise destroy w; w)

external mqtt_set_basic_auth : mq -> string -> string -> (unit, [>`EUnix of Unix.error]) result = "mqtt_set_basic_auth"

let set_basic_auth w = mqtt_set_basic_auth w.mq

external mqtt_connect : mq -> string -> int -> int -> (unit, [>`EUnix of Unix.error]) result = "mqtt_connect"

let connect w = mqtt_connect w.mq

external mqtt_disconnect : mq -> (unit, [>`EUnix of Unix.error]) result = "mqtt_disconnect"

let disconnect w = mqtt_disconnect w.mq

external mqtt_reconnect : mq -> (unit, [>`EUnix of Unix.error]) result = "mqtt_reconnect"

let reconnect w = mqtt_reconnect w.mq

external mqtt_publish : mq -> Message.t -> (unit, [>`EUnix of Unix.error]) result = "mqtt_publish"

let publish w = mqtt_publish w.mq

external mqtt_subscribe : mq -> string -> int -> (unit, [>`EUnix of Unix.error]) result = "mqtt_subscribe"

let subscribe w = mqtt_subscribe w.mq

let set_callback_message w cb =
  w.cb_message <- cb

let set_callback_connect w cb =
  w.cb_connect <- cb

let set_callback_disconnect w cb =
  w.cb_disconnect <- cb

let set_callback_publish w cb =
  w.cb_publish <- cb

let set_callback_subscribe w cb =
  w.cb_subscribe <- cb

let set_callback_unsubscribe w cb =
  w.cb_unsubscribe <- cb

let set_callback_log w cb =
  w.cb_log <- cb

external mqtt_loop : mq -> int -> int -> (unit, [>`EUnix of Unix.error]) result = "mqtt_loop"

let loop w = mqtt_loop w.mq

external mqtt_loop_forever : mq -> int -> int -> (unit, [>`EUnix of Unix.error]) result = "mqtt_loop_forever"

let loop_forever w = mqtt_loop_forever w.mq

external mqtt_socket : mq -> Unix.file_descr = "mqtt_socket"

let socket w = mqtt_socket w.mq

external mqtt_loop_read : mq -> int -> (unit, [>`EUnix of Unix.error]) result = "mqtt_loop_read"

let loop_read w = mqtt_loop_read w.mq

external mqtt_loop_write : mq -> int -> (unit, [>`EUnix of Unix.error]) result = "mqtt_loop_write"

let loop_write w = mqtt_loop_write w.mq

external mqtt_loop_misc : mq -> (unit, [>`EUnix of Unix.error]) result = "mqtt_loop_misc"

let loop_misc w = mqtt_loop_misc w.mq
