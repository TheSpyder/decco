open Migrate_parsetree;
open Ast_402;
open Ppx_tools_402;
open Ast_mapper;
open Parsetree;
open Ast_helper;
open Codecs;
open Utils;

let generateVariantEncoderCase = ({ pcd_name: { txt: name }, pcd_args, pcd_loc }) => {
    let lhsVars = switch pcd_args {
        | [] => None
        | [_] => Some(Pat.var(Location.mknoloc("v0")))
        | _ => pcd_args
            |> List.mapi((i, _) =>
                Location.mkloc("v" ++ string_of_int(i), pcd_loc) |> Pat.var)
            |> Pat.tuple
            |> (v) => Some(v)
    };

    let constructorExpr = Exp.constant(Asttypes.Const_string(name, None));

    let rhsArray = pcd_args
        |> List.map(({ ptyp_desc }) => generateCodecs(ptyp_desc, pcd_loc))
        |> List.map(((encoder, _)) => encoder) /* TODO: refactor */
        |> List.mapi((i, e) => Exp.apply(~loc=pcd_loc, e, [("", makeIdentExpr("v" ++ string_of_int(i)))]))
        |> List.append([[%expr Js.Json.string([%e constructorExpr])]])
        |> Exp.array;

    {
        pc_lhs: Pat.construct(Ast_convenience.lid(name), lhsVars),
        pc_guard: None,
        pc_rhs: [%expr Js.Json.array([%e rhsArray])]
    }
};

let indexConst = (i) =>
    Asttypes.Const_string("[" ++ string_of_int(i) ++ "]", None)
        |> Exp.constant;

let generateVariantDecodeErrorCase = (numArgs, i, _) => {
    pc_lhs:
        Array.init(numArgs, which =>
            which === i ? [%pat? Error(e)] : [%pat? _]
        )
        |> Array.to_list
        |> tupleOrSingleton(Pat.tuple),
    pc_guard: None,
    pc_rhs: [%expr Error({ ...e, path: [%e indexConst(i)] ++ e.path })]
};

let generateVariantDecodeSuccessCase = (numArgs, constructorName) => {
    pc_lhs:
        Array.init(numArgs, i =>
            Location.mknoloc("v" ++ string_of_int(i))
                |> Pat.var
                |> (p) => Pat.construct(Ast_convenience.lid("Ok"), Some(p))
        )
        |> Array.to_list
        |> tupleOrSingleton(Pat.tuple),
    pc_guard: None,
    pc_rhs:
        Array.init(numArgs, i => makeIdentExpr("v" ++ string_of_int(i)))
            |> Array.to_list
            |> tupleOrSingleton(Exp.tuple)
            |> (v) => Some(v)
            |> Exp.construct(Ast_convenience.lid(constructorName))
            |> (v) => Some(v)
            |> Exp.construct(Ast_convenience.lid("Ok"))
};

let generateVariantArgDecoder = (args, constructorName) => {
    let numArgs = List.length(args);
    args
        |> List.mapi(generateVariantDecodeErrorCase(numArgs))
        |> List.append([generateVariantDecodeSuccessCase(numArgs, constructorName)])
        |> Exp.match(args
            |> List.map(({ ptyp_desc, ptyp_loc }) => generateCodecs(ptyp_desc, ptyp_loc))
            |> List.mapi((i, (_, decoder)) => Exp.apply(decoder, [ ("", {
                let idx = Asttypes.Const_int(i + 1) /* +1 because index 0 is the constructor */
                    |> Exp.constant;

                [%expr jsonArr[[%e idx]]];
            })]))
            |> tupleOrSingleton(Exp.tuple)
        );
};

let generateVariantDecoderCase = ({ pcd_name: { txt: name }, pcd_args }) => {
    let argLen = Asttypes.Const_int(List.length(pcd_args) + 1)
        |> Exp.constant;

    let decoded = switch(pcd_args) {
        | [] => {
            let ident = Longident.parse(name) |> Location.mknoloc;
            [%expr Ok([%e Exp.construct(ident, None)])]
        }
        | _ => generateVariantArgDecoder(pcd_args, name)
    };

    {
        pc_lhs: Asttypes.Const_string(name, None)
            |> Pat.constant
            |> (v) => Some(v)
            |> Pat.construct(Ast_convenience.lid("Js.Json.JSONString")),
        pc_guard: None,
        pc_rhs: [%expr
            (Js.Array.length(tagged) !== [%e argLen]) ?
                Decco.error("Invalid number of arguments to variant constructor", v)
            :
                [%e decoded]
        ]
    }
};

let generateVariantCodecs = (constrDecls) => {
    let encoder = List.map(generateVariantEncoderCase, constrDecls)
        |> Exp.match([%expr v])
        |> Exp.fun_("", None, [%pat? v]);

    let decoderDefaultCase = {
        pc_lhs: [%pat? _],
        pc_guard: None,
        pc_rhs: [%expr Decco.error("Invalid variant constructor", jsonArr[0])]
    };
    let decoderSwitch = List.map(generateVariantDecoderCase, constrDecls)
        |> (l) => l @ [ decoderDefaultCase ]
        |> Exp.match([%expr tagged[0]]);

    let decoder = [%expr (v) =>
        switch (Js.Json.classify(v)) {
            | Js.Json.JSONArray(jsonArr) => {
                let tagged = Js.Array.map(Js.Json.classify, jsonArr);
                [%e decoderSwitch]
            }
            | _ => Decco.error("Not a variant", v)
        }
    ];

    (encoder, decoder);
};