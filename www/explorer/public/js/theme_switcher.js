var lightTheme = document.getElementsByTagName('link')[0];
var darkTheme = document.getElementsByTagName('link')[1];


function setThemeDark(){
	lightTheme.disabled = true;
	darkTheme.disabled = false;
	window.localStorage.setItem('selected_theme', 'dark');
}

function setThemeLight(){
	lightTheme.disabled = false;
	darkTheme.disabled = true;
	window.localStorage.setItem('selected_theme', 'light');
}


function chooseInitalTheme(){
	let selected_theme = window.localStorage.getItem('selected_theme');
	console.log(selected_theme);

	if (selected_theme == 'dark'){
		setThemeDark();
	} else if (selected_theme == 'light'){
		setThemeLight();
	} else {
		const darkThemeMq = window.matchMedia("(prefers-color-scheme: dark)");
		if (darkThemeMq.matches) {
			setThemeDark();
		} else {
			setThemeLight();
		}
	}
}


chooseInitalTheme();
