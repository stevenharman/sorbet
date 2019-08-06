# typed: strict
# disable-fast-path: true
A = String.new # error: Constants must have type annotations with `T.let` when specifying `# typed: strict`
  # ^^^^^^^^^^ error: Suggested type for constant without type annotation: `String`
B = T.let(T.unsafe(nil), T.untyped)

C = T.let(1, Integer)

D = T.type_alias(Integer)
