/// <reference path="_weather-interface.js" />
var _location;

function xhrRequest(url, type, callback, context) {
	var xhr = new XMLHttpRequest();
	xhr.onload = function() {
		callback.call(context || this, this.responseText);
	};
	xhr.open(type, url);
	xhr.send();
}

function messageSuccess(e) {
	//console.log('Weather info sent to Pebble successfully!');
}
function messageError(e) {
	console.log('Error sending weather info to Pebble!');
}

function locationSuccess(pos) {
	_location = pos.coords;
	//console.log(JSON.stringify(_location));
	getWeather();
}

function locationError(err) {
	console.log('Error requesting location!');
}

function getLocation() {
	//console.log('Getting current position...');
	navigator.geolocation.getCurrentPosition(
		  locationSuccess, locationError,
	  { timeout: 15000, maximumAge: 300000 }//5 minutes
	);
}

function parseColor(hex) {
	return parseInt(hex.replace(/^\#/, "0x")) || 0;
}

function loadConfig() {
	var config = localStorage.config;
	if(config) {
		//console.log("loadConfig", config);
		config = JSON.parse(config);
		Pebble.sendAppMessage({
			KEY_COLOR_BG: parseColor(config.bg || "#000000"),
			KEY_COLOR_TIME: parseColor(config.time || "#FFFFFF"),
			KEY_COLOR_TEMP: parseColor(config.temp || "#00FFFF"),
			KEY_COLOR_DAY: parseColor(config.day || "#FFAA00"),
			KEY_COLOR_DATE: parseColor(config.date || "#FFFFFF"),
			KEY_COLOR_AMPM: parseColor(config.ampm || "#FFAA00"),
			KEY_COLOR_WEATHER: parseColor(config.weather || "#FFAA00"),
			KEY_COLOR_BATTERY: parseColor(config.battery || "#000000")
		});
	}
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready', function(e) {
	//console.log('Getting weather...');
	// Get the initial location
	loadConfig();
	getLocation();
});

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage', function(e) {
	//console.log('AppMessage received!');
	if(!_location || (e.payload && e.payload["50"]))
		getLocation();
	else
		getWeather();
});

Pebble.addEventListener('showConfiguration', function(e) {
	// Show config page
	Pebble.openURL('http://public.dev.dsi.data-systems.com/alan/pebble/config.html');
});

Pebble.addEventListener('webviewclosed', function(e) {
	var config = decodeURIComponent(e.response);
	//console.log('Configuration window returned: ' + config);
	localStorage.config = config;
	loadConfig();
});
/// <reference path="_base.js" />
function getWeather() {
	// We will request the weather here
	var url = "https://api.forecast.io/forecast/a5f7eaee6f997519a920c1c3fcf9117e/"
		+ _location.latitude + "," + _location.longitude;
	xhrRequest(url, "GET", weatherResult);
}

var _name = "Hero",
	_shortMap = {
		"clear-day": "*",
		"clear-night": "*",
		"rain": "Ra",
		"snow": "Sn",
		"sleet": "Sl",
		"wind": "Wi",
		"fog": "Fo",
		"cloudy": "Cl",
		"partly-cloudy-day": "Pc",
		"partly-cloudy-night": "Pc"
	}, _longMap = {
		"clear-day": " clears his mind.",
		"clear-night": " sees stars.",
		"rain": " sprays water.",
		"snow": " casts Blizzard.",
		"sleet": " sees stars.",
		"wind": " takes a deep breath.",
		"fog": " is confused.",
		"cloudy": " is lost in the clouds.",
		"partly-cloudy-day": " is mostly here.",
		"partly-cloudy-night": " is half asleep."
	}, _day = [
		"Sun",
		"Mon",
		"Tue",
		"Wed",
		"Thu",
		"Fri",
		"Sat"
	];

function weatherResult(responseText) {
	// responseText contains a JSON object with weather info
	var json = JSON.parse(responseText),
		current = json.currently,
		daily = json.daily.data;
	//console.log(responseText);

	Pebble.sendAppMessage({
		KEY_MIN: Math.round(daily[0].temperatureMin),
		KEY_MAX: Math.round(daily[0].temperatureMax),
		KEY_CON: _shortMap[daily[0].icon],
		KEY_PREC: Math.round(daily[0].precipProbability * 100),
		KEY_TOM: _day[new Date(daily[1].time * 1000).getDay()],
		KEY_TOM_MIN: Math.round(daily[1].temperatureMin),
		KEY_TOM_MAX: Math.round(daily[1].temperatureMax),
		KEY_TOM_CON: _shortMap[daily[1].icon],
		KEY_TOM_PREC: Math.round(daily[1].precipProbability * 100),
		KEY_MESSAGE: _name + _longMap[current.icon] + "\n"
			+ Math.round(current.temperature) + " HP lost.\n"
			+ json.minutely.summary || json.daily.summary
	}, messageSuccess, messageError);
}