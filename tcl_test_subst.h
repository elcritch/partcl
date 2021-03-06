#ifndef TCL_TEST_SUBST_H
#define TCL_TEST_SUBST_H
#ifdef TCL_TEST

static void test_subst()
{
  printf("\n");
  printf("###################\n");
  printf("### SUBST TESTS ###\n");
  printf("###################\n");
  printf("\n");

  check_eval(NULL, "subst hello", "hello");
  check_eval(NULL, "subst {hello}", "hello");
  check_eval(NULL, "subst {hello world}", "hello world");
  check_eval(NULL, "subst {hello {world}}", "hello {world}");

  check_eval(NULL, "subst $foo", "");

  struct tcl tcl;
  tcl_init(&tcl);
  tcl_var(&tcl, "foo", tcl_alloc("bar", 3));
  tcl_var(&tcl, "bar", tcl_alloc("baz", 3));
  tcl_var(&tcl, "baz", tcl_alloc("Hello", 5));

  check_eval(&tcl, "subst $foo", "bar");
  check_eval(&tcl, "subst $foo[]$foo", "barbar");
  check_eval(&tcl, "subst $$foo", "baz");
  check_eval(&tcl, "subst [set $foo]", "baz");
  check_eval(&tcl, "subst $[set $foo]", "Hello");
  check_eval(&tcl, "subst $$$foo", "Hello");
  tcl_destroy(&tcl);

  check_eval(NULL, "subst {hello}{world}", "helloworld");
  check_eval(NULL, "subst hello[subst world]", "helloworld");
  check_eval(NULL, "subst hello[\n]world", "helloworld");

  /* Example from Picol */
  check_eval(NULL, "set a su; set b bst; $a$b Hello", "Hello");
  /* This is an error in TCL, but works in Picol */
  check_eval(NULL, "set foo {hello world}", "hello world");
  check_eval(NULL,
      "set foo {hello world}; set bar \"qux $foo\"; subst $foo$bar",
      "hello worldqux hello world");
  check_eval(NULL, "set a f; set b {o}; set $a$b$b [subst \"hello\"]; set foo",
      "hello");
  check_eval(NULL, "set {a \"b\"} hello; subst ${a \"b\"}", "hello");
  check_eval(NULL, "set \"a b\" hello; subst ${a b}", "hello");

  check_eval(NULL, "set q {\"}; set msg hello; subst $q$msg$q", "\"hello\"");
  check_eval(NULL, "set q {\"}; subst $q[]hello[]$q", "\"hello\"");
  check_eval(NULL, "set x {\n\thello\n}", "\n\thello\n");

  /* Some puts commands */
  check_eval(NULL, "puts {[}[]hello[]{]}", "[hello]");
  check_eval(NULL, "puts {{hello}}", "{hello}");

  /* XXX most command involving unpaired braces (e.g. in quotes) don't work
   * because of the dirty list implementation */
}
#endif

#endif /* TCL_TEST_SUBST_H */
