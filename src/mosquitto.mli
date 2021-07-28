type t

module Message : sig
  type t
  val create : ?mid:int -> topic:string -> ?qos:int -> ?retain:bool -> string -> t
  val mid : t -> int option
  val topic : t -> string
  val qos : t -> int
  val retain : t -> bool
  val payload : t -> string
end

module Version : sig
  val version : int
  val major : int
  val minor : int
  val revision : int
end

val create : string -> bool -> (t, [>`EUnix of Unix.error]) Result.result

val destroy : t -> unit

val set_basic_auth : t -> string -> string -> (unit, [>`EUnix of Unix.error]) Result.result

val connect : t -> string -> int -> int -> (unit, [>`EUnix of Unix.error]) Result.result

val reconnect : t -> (unit, [>`EUnix of Unix.error]) Result.result

val disconnect : t -> (unit, [>`EUnix of Unix.error]) Result.result

val publish : t -> Message.t -> (unit, [>`EUnix of Unix.error]) Result.result

val subscribe : t -> string -> int -> (unit, [>`EUnix of Unix.error]) Result.result

val set_callback_message : t -> (Message.t -> unit) -> unit

val set_callback_connect : t -> (int -> unit) -> unit

val set_callback_disconnect : t -> (int -> unit) -> unit

val set_callback_publish : t -> (int -> unit) -> unit

val set_callback_subscribe : t -> (int -> int list -> unit) -> unit

val set_callback_unsubscribe : t -> (int -> unit) -> unit

val set_callback_log : t -> (int -> string -> unit) -> unit

val loop : t -> int -> int -> (unit, [>`EUnix of Unix.error]) Result.result

val loop_forever : t -> int -> int -> (unit, [>`EUnix of Unix.error]) Result.result

val socket : t -> Unix.file_descr

val loop_read : t -> int -> (unit, [>`EUnix of Unix.error]) Result.result

val loop_write : t -> int -> (unit, [>`EUnix of Unix.error]) Result.result

val loop_misc : t -> (unit, [>`EUnix of Unix.error]) Result.result
