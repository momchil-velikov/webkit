if (!errorMessage)
    successfullyParsed = true;
shouldBeTrue("successfullyParsed");
debug('<br /><span class="pass">TEST COMPLETE</span>');

if (window.layoutTestController)
    layoutTestController.notifyDone();
