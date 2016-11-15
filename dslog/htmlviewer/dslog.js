$(function() {
	var ws = new WebSocket("ws://localhost:9080/dslogger");

	var $doc = $(document);
	var $wnd = $(window);
	
	var template = $('#client-template');
	var counter = document.getElementById('counter');

	function toggleClient() {
		var bts = $('#all-clients').children('button');
		var logs = $('div.log-content');

		for(var i = 0; i < bts.length; ++ i) {
			$(logs[i]).css('display', (bts[i] == this) ? 'block' : 'none');
		}

		$doc.scrollTop($doc.height());
	}

	function clearClient() {
		var $c = $(this).parent();
		$c.find('table:first').remove();
		$c.append(template.find('table').clone());
	}

	function getClientContent(cltId) {
		var clt = $('#' + cltId);
		if (clt.length == 0) {
			clt = $(document.createElement('div')).append(template.children().clone()).appendTo($('body')).attr('id', cltId).addClass('log-content');
			$('#all-clients').append('<button title="点击切换" id="bt-' + cltId + '">Client:' + cltId + '</button>')
				.find('button:last').click(toggleClient);

			clt.find('button:first').click(clearClient);
		}

		return clt;
	}

	function showMask() {
		if (!$('.loading-background').length) {
			var $bd = $('body');
			$(document.createElement('div')).addClass('loading-background').appendTo($bd).unbind();
			$(document.createElement('div')).addClass('loading-container').appendTo($bd).unbind().html('日志服务端未连接……');
		}
	}

	showMask();

	ws.onopen = function()
	{
		$('.loading-background,.loading-container').remove();
		console.log('connected');
	};

	ws.onmessage = function (evt) 
	{
		var txt = evt.data;
		var idEnd = txt.indexOf('|');
		var id = txt.substring(0, idEnd);		

		txt = txt.substr(idEnd + 1);
		var a = txt.indexOf('(');
		var b = txt.indexOf(')', a + 1);

		if (txt.charCodeAt(0) == 42) {
			// * at first
			var $c = $('#' + id);
			var cmd = txt.substring(1, a);
			if (cmd == 'clear') {
				// clear 
				if ($c.find('input:checkbox:first').prop('checked')) {
					$c.find('table:first').remove();
					$c.append(template.find('table').clone());
				}

			} else if (cmd == 'close') {
				// client disconnected
				// $('#bt-' + id).remove();
				// $c.remove();
			}

		} else {			
			var $c = getClientContent(id);
			var row = $c.find('table:first')[0].insertRow(-1);

			$(document.createElement('pre')).html(txt.substr(0, a)).appendTo(row.insertCell(-1));
			$(document.createElement('pre')).html(txt.substring(a + 1, b)).appendTo(row.insertCell(-1));
			$(document.createTextNode(txt.substr(b + 1))).appendTo(row.insertCell(-1));
		}

		$doc.scrollTop($doc.height());
	};

	ws.onclose = function()
	{
		showMask();
		console.log('close');
	};
});