/*
 * Qt_GUI.cpp
 *
 *  Created on: Jan 22, 2025
 *      Author: mad
 */

#include <mmx/Qt_GUI.h>
#include <vnx/vnx.h>

#ifdef WITH_QT

#include <QIcon>
#include <QApplication>
#include <QWebEngineView>
#include <QLoggingCategory>
#include <QWebEngineSettings>
#include <QWebEngineProfile>
#include <QWebEngineScript>
#include <QWebEngineScriptCollection>
#include <QWebEngineUrlRequestInterceptor>


class RequestInterceptor : public QWebEngineUrlRequestInterceptor {
public:
	std::string api_host;
	std::string api_token;
	std::string api_token_header;
	explicit RequestInterceptor(QObject* parent = nullptr) : QWebEngineUrlRequestInterceptor(parent) {}
	virtual ~RequestInterceptor() = default;
	void interceptRequest(QWebEngineUrlRequestInfo& info) override {
		const auto host = info.requestUrl().host().toStdString() + ":" + std::to_string(info.requestUrl().port());
		if(host == api_host) {
			info.setHttpHeader(QByteArray::fromStdString(api_token_header), QByteArray::fromStdString(api_token));
		}
	}
};

void qt_log_func(QtMsgType type, const QMessageLogContext &context, const QString &msg) {
    vnx::log_debug() << "QT: " << msg.toStdString() << " (" << context.file << ":" << context.line << ")";
}

void qt_gui_exec(char** argv, std::string host, std::string api_token, std::string api_token_header)
{
	qInstallMessageHandler(qt_log_func);

	const auto full_url = "http://" + host + "/gui/";

	int argc = 1;
	QApplication app(argc, argv);

	const auto interceptor = new RequestInterceptor();
	interceptor->api_host = host;
	interceptor->api_token = api_token;
	interceptor->api_token_header = api_token_header;

	QWebEngineScript script;
	script.setSourceCode("window.mmx_qtgui = true;");
	script.setInjectionPoint(QWebEngineScript::DocumentCreation);
	script.setWorldId(QWebEngineScript::MainWorld);

	QWebEngineView view;
	view.page()->profile()->setRequestInterceptor(interceptor);
	view.page()->settings()->setAttribute(QWebEngineSettings::JavascriptCanAccessClipboard, true);
	view.page()->settings()->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
	view.page()->settings()->setAttribute(QWebEngineSettings::LocalContentCanAccessRemoteUrls, true);
	view.page()->scripts().insert(script);
	view.setUrl(QUrl(QString::fromStdString(full_url)));
	view.setWindowTitle("MMX Node");
	view.setWindowIcon(QIcon("www/web-gui/public/assets/img/logo_circle_color_cy256.png"));
	view.resize(1300, 1000);
	view.show();

	app.exec();
}



#endif // WITH_QT
