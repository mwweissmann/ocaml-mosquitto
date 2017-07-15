let get_ok = function | Result.Ok x -> x | Result.Error (`EUnix e) -> failwith (Unix.error_message e)

let _ =
  let mqtt = get_ok (Mosquitto.create "ocaml-pub" false) in
  let () = get_ok (Mosquitto.connect mqtt "127.0.0.1" 1883 0) in
  let rec loop = function
    | 0 -> print_endline "done"; print_endline "publish"
    | n ->
      let msg = Mosquitto.Message.create ~topic:Sys.argv.(1) (Printf.sprintf "%s[%d]" Sys.argv.(2) n) in
      let () = get_ok (Mosquitto.publish mqtt msg) in
      loop (n - 1)
  in
  loop 10000
; Unix.sleep 2
