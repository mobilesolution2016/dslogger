$(function() {
	var ws = new WebSocket("ws://localhost:9080/dslogger");

	var $doc = $(document);
	var $wnd = $(window);
	
	var counter = document.getElementById('counter');
				
	ws.onopen = function()
	{
		console.log('connected');
	};

	ws.onmessage = function (evt) 
	{
		var txt = evt.data;
		var log = document.getElementById('content');

		var a = txt.indexOf('(');
		if (txt.charCodeAt(0) == 42) {
			var cmd = a ? txt.substr(1) : txt.substring(1, a);
			if (cmd == 'clear') {
				log.remove();
				$('#template').clone().attr('id', 'content').show().appendTo($('body'));
			}

		} else {
			var row = log.insertRow(-1);
			var b = txt.indexOf(')', a + 1);
			var c = txt.indexOf(':', b + 1);

			$(document.createElement('pre')).html(txt.substr(0, a)).appendTo(row.insertCell(-1));
			$(document.createElement('pre')).html(txt.substring(a + 1, b)).appendTo(row.insertCell(-1));
			$(document.createElement('pre')).html(txt.substring(b + 1, c)).appendTo(row.insertCell(-1));
			$(document.createTextNode(txt.substr(c + 1))).appendTo(row.insertCell(-1));
		}

		$doc.scrollTop($doc.height());
		counter.innerHTML = log.rows.length - 1;
	};

	ws.onclose = function()
	{ 
		console.log('close');
	};
});