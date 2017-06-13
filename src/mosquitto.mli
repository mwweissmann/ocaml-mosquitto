type t

type msg = {
  mid : int;
  topic : string;
  payload : string;
  qos : int;
  retain : bool;
}

val create : string -> bool -> (t, [>`EUnix of Unix.error]) Result.result

val connect : t -> string -> int -> int -> (unit, [>`EUnix of Unix.error]) Result.result

val reconnect : t -> (unit, [>`EUnix of Unix.error]) Result.result

val publish : t -> string -> string -> int -> bool -> (unit, [>`EUnix of Unix.error]) Result.result

val subscribe : t -> string -> int -> (unit, [>`EUnix of Unix.error]) Result.result

val callback_set : t -> (msg -> unit) -> unit

val loop : t -> int -> int -> (unit, [>`EUnix of Unix.error]) Result.result

