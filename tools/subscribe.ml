let get_ok = function | Result.Ok x -> x | Result.Error (`EUnix e) -> failwith (Unix.error_message e)

let _ =
  let mqtt = get_ok (Mosquitto.create "horch" false) in
  let () = get_ok (Mosquitto.connect mqtt "127.0.0.1" 1883 0) in

  let cb msg = Printf.printf "%s: '%s'\n%!" Mosquitto.(msg.topic) Mosquitto.(msg.payload) in
  let () = Mosquitto.callback_set mqtt cb in

  let () = get_ok (Mosquitto.subscribe mqtt Sys.argv.(1) 0) in

  let rec loop () =
    match (Mosquitto.loop mqtt 1000 1) with
    | Result.Ok () -> loop ()
    | Result.Error (`EUnix e) ->
      let () = print_endline (Unix.error_message e) in
      reconnect ()
  and reconnect () =
    let () = Unix.sleep 1 in
    match Mosquitto.reconnect mqtt with
    | Result.Ok () -> loop ()
    | Result.Error (`EUnix e) -> 
      let () = print_endline (Unix.error_message e) in
      reconnect ()
  in
  loop ()


