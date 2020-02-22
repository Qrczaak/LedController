function onColorPicker(){
  var colorPicker = document.getElementById("color-picker");
  var hiddenSolidColorInput = document.getElementById("hidden-input-solid-color");
  var hiddenColorInput = document.getElementById("hidden-input-color");
  var color = colorPicker.value;

  hiddenColorInput.value = color.substr(1);
  hiddenSolidColorInput.value = color.substr(1);
}


var brightnessSlider = document.getElementById("brightnessSlider");
var brightnessOutput = document.getElementById("brightnessValue");
brightnessOutput.innerHTML = brightnessSlider.value; // Display the default slider value

// Update the current slider value (each time you drag the slider handle)
brightnessSlider.oninput = function() {
  brightnessOutput.innerHTML = this.value;
}

var delaySlider = document.getElementById("delaySlider");
var delayOutput = document.getElementById("delayValue");
delayOutput.innerHTML = delaySlider.value; // Display the default slider value

// Update the current slider value (each time you drag the slider handle)
delaySlider.oninput = function() {
  delayOutput.innerHTML = this.value;
}
