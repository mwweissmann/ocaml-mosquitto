let get_ok = function | Result.Ok x -> x | Result.Error (`EUnix e) -> failwith (Unix.error_message e)

let _ =
  let mqtt = get_ok (Mosquitto.create "august" true) in
  let cb msg = Printf.printf "[DATA] %s: '%s'\n%!" (Mosquitto.Message.topic msg) (Mosquitto.Message.payload msg) in
  let () = Mosquitto.set_callback_message mqtt cb in
  let () = get_ok (Mosquitto.connect mqtt "127.0.0.1" 1883 1000) in

  let () = get_ok (Mosquitto.subscribe mqtt Sys.argv.(1) 0) in

  let _ = Mosquitto.loop_forever mqtt 1000 1 in
  let _ = failwith "ooooops" in

  let rec loop () =
    let fd = Mosquitto.socket mqtt in
    let () = match Unix.select [fd] [] [] 5.0 with
      | [], [], [] -> print_endline "timeout"
      | _ -> print_endline "new data!"
    in
    match (Mosquitto.loop mqtt 1000 1) with
    | Result.Ok () -> loop ()
    | Result.Error (`EUnix Unix.EINVAL) -> print_endline "EINVAL"; loop ()
    | Result.Error (`EUnix e) ->
      let () = Printf.fprintf stderr "oops1: %s\n%!" (Unix.error_message e) in
      reconnect ()
  and reconnect () =
    print_endline "reconncting in 1s...";
    let () = Unix.sleep 1 in
    match Mosquitto.reconnect mqtt with
    | Result.Ok () -> loop ()
    | Result.Error (`EUnix e) -> 
      let () = Printf.fprintf stderr "oops2: %s\n%!" (Unix.error_message e) in
      reconnect ()
  in
  loop ()


