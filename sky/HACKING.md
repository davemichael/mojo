Hacking on Sky
==============

Building
--------

* Follow the setup & build instructions for [Mojo](https://github.com/domokit/mojo)

Running applications
--------------------

* ``./sky/tools/skydb --debug [url]``
  * You should see a ``(skydb)`` prompt
  * Type ``help`` to see the list of available commands
  * Note: skydb is currently hard-coded to use ``//out/Debug``

* ``./sky/tools/test_sky --debug``
  * This runs the tests against ``//out/Debug``. If you want to run against
    ``//out/Release``, omit the ``--debug`` flag.

Running tests manually
----------------------

* ``sky/tools/run_sky_httpd``
* ``out/Debug/mojo_shell --args-for="mojo:native_viewport_service --use-headless-config" --content-handlers=text/html,mojo:sky_viewer --url-mappings=mojo:window_manager=mojo:sky_tester mojo:window_manager``
* The ``sky_tester`` should print ``#READY`` when ready
* Type the URL you wish to run, for example ``http://127.0.0.1:8000/lowlevel/text.html``, and press the enter key
* The harness should print the results of the test.  You can then type another URL.

Writing tests
-------------

* Import ``resources/mocha.html`` and ``resources/chai.html``
* Write tests in [mocha format](http://visionmedia.github.io/mocha/#getting-started) and use [chai asserts](http://chaijs.com/api/assert/):
```html
describe('My pretty test of my subject', function() {
  var subject = new MySubject();

  it('should be pretty', function() {
    assert.ok(subject.isPretty);
  });
});
```
