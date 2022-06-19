function ColorManager(predefinedColors) {
	this.availableColors = predefinedColors,
	this.lookupTable = [],
		
	this.getColor = function(key) {
		for (var i = 0; i < this.lookupTable.length; ++i) {
			var item = this.lookupTable[i];
			
			if (item.key == key)
				return item.color;
		}
		
		// get a new color from availableColors
		if (this.availableColors.length == 0)
			return null;
		
		var color = this.availableColors[0];
		this.availableColors.splice(0, 1); // remove
		
		var item = { };
		item.key = key;
		item.color = color;
		this.lookupTable.push(item);
		
		return color;
	}
};

function getFilename(str) {
	var n = str.indexOf(':');
	if (n != -1)
		return str.substr(0, n);
	
	return str;
}

var originalHtml = null;
var filterHexMessages = null;

function removeFilteredRows() {
	if (!filterHexMessages)
		return;
	
	var find = filterHexMessages.toLowerCase();
	
	$('table tbody tr td:nth-child(4)').each(function(x, y) { // message
		var html = $(y).html().toLowerCase();
		if (html.indexOf(find) != -1) {
			//$(y).parent().remove();
			$(y).parent().css('background', '#8f4500');
		}
	})
}

function processTable() {
	removeFilteredRows();
	
	var threadColorManager = new ColorManager(['#0cb000', '#1326bc', '#b00000', '#c0a000', '#00c0c0', '#af26cf', '#aac006']);
	var fileColorManager = new ColorManager(['#0cb000', '#00a6fc', '#e00000', '#c0a000', '#00c0c0', '#af26cf', '#aac006']);

	$('table tbody tr td:nth-child(2)').each(function(x, y) { // thread
		var thread = parseInt($(y).html());
		
		var html = $(y).html();
		
		var color = threadColorManager.getColor(thread);
		if (color) {
			
			html = '<div class="thread-badge" style="background: ' + color + '">' + html + '</div>';
			
			$(y).html(html);
		}
		else
			console.log('failed to get color for thread: ' + thread);
	})
	
	$('table tbody tr td:nth-child(3)').each(function(x, y) { // file
		
		var html = $(y).html();
	
		var filename = getFilename(html);
		var color = fileColorManager.getColor(filename);
		
		if (color) {
			//html = '<div class="thread-badge" style="background: ' + color + '">' + html + '</div>';
			//$(y).html(html);
			$(y).css('color', color);
		}
		else
			console.log('failed to get color from file: ' + filename);
	})

	$('table tbody tr td:nth-child(4)').each(function(x, y) { // message
		
		var html = $(y).html();
		var htmlLowerCase = html.toLowerCase();
		
		if (htmlLowerCase.indexOf('error') != -1 ||
			htmlLowerCase.indexOf('fail') != -1) {
			$(y).css('color', '#ff0000');
		}
		else if (htmlLowerCase.indexOf('warning') != -1 ||
			htmlLowerCase.indexOf('unknown') != -1) {
			$(y).css('color', '#dfdf00');
		}
		else if (htmlLowerCase.indexOf('allocate') != -1 ||
			htmlLowerCase.indexOf('destroy') != -1) {
			$(y).css('color', '#f0c59b');
		}
		else if (htmlLowerCase.indexOf('cancel') != -1) {
			$(y).css('color', '#faa08c');
		}
		
		const NumberOfPointerHex = 16;
		var pattern = /([a-f0-9]{16})/i;
		
		var n = 0;
		var searchIndex = 0;
		while ((n = html.substr(searchIndex).search(pattern)) != -1) {
			var hex = html.substr(searchIndex + n, NumberOfPointerHex);
			
			var url = '<a href="#/filter=' + hex + '" class="address-link">' + hex + '</a>';

			html = html.substr(0, searchIndex + n) + url + html.substr(searchIndex + n + NumberOfPointerHex);
			searchIndex = searchIndex + n + url.length;
			$(y).html(html);
		}
	})
}

function resetTable() {
	$('table').html(originalHtml);
	
	// fix message so they are in a span
	/*$('table tbody tr td:nth-child(4)').each(function(x, y) { // message
		
		var html = $(y).html();
		$(y).html('<span class="overflow-check">' + html + '</span>');
	})*/
	
	processTable();
}

function addressClick(addr) {
	filterHexMessages = addr;
	resetTable();
	return false;
}

function checkHash() {
	
	var filter = '';
	
	var hash = document.location.hash;
	if (hash.indexOf('#/') == 0) {
		var data = hash.substr(2);
		if (data.indexOf('filter=') == 0) {
			var hex = data.substr(7);
			filter = hex;
		}
	}
	
	if (filterHexMessages != filter) {
		filterHexMessages = filter;
		resetTable(); // always change on checkHash
	}
}

function processTableColoring() {

	originalHtml = $('table').html();
	resetTable();
	checkHash();
	
	$(window).on('hashchange', function() {
		checkHash();
	})
	
	$('#aResetTable').click(function() {
		filterHexMessages = null;
		resetTable();
		document.location.hash = '';
		return false;
	})
	
}