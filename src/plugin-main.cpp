
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <plugin-support.h>

#include <QApplication>
#include <QColor>
#include <QVariant>
#include <QMainWindow>
#include <QWidget>
#include <QPalette>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QShowEvent>
#include <QResizeEvent>
#include <QPointer>
#include <QMetaObject>
#include <vector>
#include <QPainter>
#include <QPaintEvent>
#include <QFont>
#include <atomic>
#include <cmath>
#include <graphics/graphics.h>
#include <QComboBox>
#include <QPushButton>
#include <QStringList>
#include <QGuiApplication>
#include <QScreen>
#include <QShortcut>
#include <QKeySequence>
#include <QRect>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDesktopServices>
#include <QUrl>
#include <QVersionNumber>
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

#include <algorithm>
#include <cstdint>

#pragma comment(lib, "winhttp.lib")

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

// ------------------------------------------------------------
// Janela de vÃ­deo: renderiza a cena atual do programa do OBS.
// ------------------------------------------------------------

class PgmVideoWidget : public QWidget {
public:
    explicit PgmVideoWidget(QWidget *parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_NativeWindow);
        setMinimumSize(480, 270);
        setAutoFillBackground(true);
        setBackgroundRole(QPalette::Window);
    }

    ~PgmVideoWidget() override
    {
        if (display) {
            obs_display_remove_draw_callback(
                display, draw_frame, this);
            obs_display_destroy(display);
            display = nullptr;
        }
    }

protected:
    void showEvent(QShowEvent *event) override
    {
        QWidget::showEvent(event);
        create_display();
    }

    void resizeEvent(QResizeEvent *event) override
    {
        QWidget::resizeEvent(event);

        if (display) {
            obs_display_resize(
                display,
                static_cast<uint32_t>(std::max(1, width())),
                static_cast<uint32_t>(std::max(1, height())));
        }
    }

private:
    obs_display_t *display = nullptr;

    uint32_t get_obs_theme_background_color() const
    {
        QColor color = palette().color(QPalette::Window);
        auto *obsWindow = static_cast<QWidget *>(obs_frontend_get_main_window());

        if (obsWindow) {
            const auto widgets = obsWindow->findChildren<QWidget *>();

            for (QWidget *widget : widgets) {
                if (widget->metaObject()->indexOfProperty("displayBackgroundColor") >= 0) {
                    const QVariant value = widget->property("displayBackgroundColor");

                    if (value.isValid() && value.canConvert<QColor>()) {
                        color = value.value<QColor>();
                        break;
                    }
                }
            }
        }

        return static_cast<uint32_t>(color.red()) |
               (static_cast<uint32_t>(color.green()) << 8) |
               (static_cast<uint32_t>(color.blue()) << 16) |
               (static_cast<uint32_t>(color.alpha()) << 24);
    }
    void create_display()
    {
        if (display || !winId())
            return;

        gs_init_data info{};

        info.window.hwnd = reinterpret_cast<void *>(winId());
        info.cx = static_cast<uint32_t>(std::max(1, width()));
        info.cy = static_cast<uint32_t>(std::max(1, height()));
        info.num_backbuffers = 2;
        info.format = GS_BGRA;
        info.zsformat = GS_ZS_NONE;

        const uint32_t backgroundColor = get_obs_theme_background_color();
        display = obs_display_create(&info, backgroundColor);

        if (!display) {
            blog(LOG_ERROR, "[PREVIEW PGM] Falha ao criar display.");
            return;
        }

        obs_display_add_draw_callback(display, draw_frame, this);
        auto *themeTimer = new QTimer(this);
        connect(themeTimer, &QTimer::timeout, this, [this]() {
            if (display)
                obs_display_set_background_color(display, get_obs_theme_background_color());
        });
        themeTimer->start(500);

        blog(LOG_INFO, "[PREVIEW PGM] Display de vÃ­deo criado.");
    }

    
static void draw_frame(void *data, uint32_t cx, uint32_t cy)
{
    Q_UNUSED(data);

    if (!cx || !cy)
        return;

    struct obs_video_info video_info{};
    if (!obs_get_video_info(&video_info))
        return;

    const uint32_t source_width = video_info.base_width;
    const uint32_t source_height = video_info.base_height;

    if (source_width && source_height) {
        const float scaleX = static_cast<float>(cx) / source_width;
        const float scaleY = static_cast<float>(cy) / source_height;
        const float scale = std::min(scaleX, scaleY);
        const float renderedWidth = source_width * scale;
        const float renderedHeight = source_height * scale;
        const float offsetX = (cx - renderedWidth) * 0.5f;
        const float offsetY = (cy - renderedHeight) * 0.5f;

        gs_projection_push();
        gs_ortho(0.0f, static_cast<float>(cx), 0.0f, static_cast<float>(cy), -100.0f, 100.0f);
        gs_matrix_push();
        gs_matrix_translate3f(offsetX, offsetY, 0.0f);
        gs_matrix_scale3f(scale, scale, 1.0f);
        struct gs_rect scissor = { static_cast<int>(offsetX), static_cast<int>(offsetY), static_cast<int>(renderedWidth), static_cast<int>(renderedHeight) };
        gs_set_scissor_rect(&scissor);
        obs_render_main_texture();
        gs_set_scissor_rect(nullptr);
        gs_matrix_pop();
        gs_projection_pop();
    }
}

};

// ------------------------------------------------------------
// Janela principal PREVIEW PGM.
// ------------------------------------------------------------


class PgmAudioMeter : public QWidget {
public:
    explicit PgmAudioMeter(QWidget *parent = nullptr)
        : QWidget(parent)
    {
setMinimumHeight(94);
setMaximumHeight(100);

        auto *timer = new QTimer(this);

        connect(timer, &QTimer::timeout, this, [this]() {
            
for (int i = 0; i < 2; ++i) {
    // Subida rÃ¡pida para acompanhar o Mixer do OBS.
    if (target_[i] >= level_[i]) {
        level_[i] = target_[i];
    } else {
        // Descida suave, com resposta mais rÃ¡pida.
        level_[i] += (target_[i] - level_[i]) * 0.45f;
    }

    // RetenÃ§Ã£o e queda gradual do pico.
    if (target_[i] >= peak_[i]) {
        peak_[i] = target_[i];
        peakHold_[i] = 25;
    } else if (peakHold_[i] > 0) {
        --peakHold_[i];
    } else {
        peak_[i] = std::max(-60.0f, peak_[i] - 0.8f);
    }
}


            update();
        });

        timer->start(30);
    }

    

void setLevels(float left, float right,
               float peakLeft, float peakRight)
{
    left = std::clamp(left, -60.0f, 0.0f);
    right = std::clamp(right, -60.0f, 0.0f);
    peakLeft = std::clamp(peakLeft, -60.0f, 0.0f);
    peakRight = std::clamp(peakRight, -60.0f, 0.0f);

    target_[0] = left;
    target_[1] = right;

    peak_[0] = std::max(peak_[0], peakLeft);
    peak_[1] = std::max(peak_[1], peakRight);

    update();
}



protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, false);
        p.fillRect(rect(), palette().color(QPalette::Window));

        const int leftMargin = 38;
        const int rightMargin = 10;
        const int barWidth = width() - leftMargin - rightMargin;

        if (barWidth <= 0)
            return;

        const int barHeight = 13;
        const int ys[2] = {8, 32};

        p.setFont(QFont("Arial", 8));

        for (int ch = 0; ch < 2; ++ch) {
            p.setPen(palette().color(QPalette::WindowText));
            p.drawText(2, ys[ch] + 11, ch == 0 ? "L" : "R");

            const int y = ys[ch];

            // Fundo da barra.
            p.fillRect(leftMargin, y, barWidth, barHeight,
                       QColor("#292929"));

            // Escala: -60 dB atÃ© 0 dB.
            auto xForDb = [&](float db) {
                return leftMargin + qRound(
                    ((db + 60.0f) / 60.0f) * barWidth);
            };

            // Faixas de cor do medidor.
            p.fillRect(leftMargin, y,
                       xForDb(-20.0f) - leftMargin, barHeight,
                       QColor("#20B957"));

            p.fillRect(xForDb(-20.0f), y,
                       xForDb(-10.0f) - xForDb(-20.0f), barHeight,
                       QColor("#E5C936"));

            p.fillRect(xForDb(-10.0f), y,
                       xForDb(0.0f) - xForDb(-10.0f), barHeight,
                       QColor("#E04444"));

            
            // Preenchimento acompanha o nÃ­vel exibido.
            const int levelX = std::clamp(
                xForDb(level_[ch]),
                leftMargin,
                leftMargin + barWidth);

            const int greenEnd = xForDb(-20.0f);
            const int yellowEnd = xForDb(-10.0f);

            // Fundo escuro.
            p.fillRect(leftMargin, y, barWidth, barHeight,
                       QColor("#292929"));

            // Faixa verde: -60 a -20 dB.
            const int greenWidth = std::max(
                0, std::min(levelX, greenEnd) - leftMargin);

            if (greenWidth > 0) {
                p.fillRect(leftMargin, y, greenWidth, barHeight,
                           QColor("#20B957"));
            }

            // Faixa amarela: -20 a -10 dB.
            const int yellowStart = std::max(leftMargin, greenEnd);
            const int yellowWidth = std::max(
                0, std::min(levelX, yellowEnd) - yellowStart);

            if (yellowWidth > 0) {
                p.fillRect(yellowStart, y, yellowWidth, barHeight,
                           QColor("#E5C936"));
            }

            // Faixa vermelha: -10 a 0 dB.
            const int redStart = std::max(leftMargin, yellowEnd);
            const int redWidth = std::max(
                0, levelX - redStart);

            if (redWidth > 0) {
                p.fillRect(redStart, y, redWidth, barHeight,
                           QColor("#E04444"));
            }


            // SegmentaÃ§Ã£o visual.
            p.setPen(QColor("#151515"));
            for (int x = leftMargin + 5;
                 x < leftMargin + barWidth; x += 6) {
                p.drawLine(x, y, x, y + barHeight - 1);
            }

            // Marcador de pico.
            const int peakX = xForDb(peak_[ch]);
            p.setPen(QPen(palette().color(QPalette::WindowText), 2));
            p.drawLine(peakX, y, peakX, y + barHeight - 1);
        }

        // MarcaÃ§Ãµes de -60 a 0 dB, de 6 em 6.
        p.setPen(palette().color(QPalette::WindowText));

        for (int db = -60; db <= 0; db += 6) {
            const int x = leftMargin + qRound(
                ((db + 60.0f) / 60.0f) * barWidth);

            p.drawLine(x, 49, x, 54);

            const QString label =
                db == 0 ? "0" : QString::number(db);

            p.drawText(x - 11, 70, 24, 14,
                       Qt::AlignHCenter, label);
        }

        p.drawText(width() - 24, 70, 22, 14,
                   Qt::AlignRight, "dB");
    }

private:
    float target_[2] = {-60.0f, -60.0f};
    float level_[2] = {-60.0f, -60.0f};
    float peak_[2] = {-60.0f, -60.0f};
    int peakHold_[2] = {0, 0};
};


class PreviewPgmWindow : public QMainWindow {
public:
    explicit PreviewPgmWindow(QWidget *parent = nullptr)
        : QMainWindow(parent)
    {
        setWindowTitle("PREVIEW PGM");
        resize(1100, 700);
        setAutoFillBackground(true);

        auto *central = new QWidget(this);
        auto *layout = new QVBoxLayout(central);
        layout->setContentsMargins(12, 12, 12, 12);
        layout->setSpacing(10);

        auto *header = new QHBoxLayout();

        auto *title = new QLabel("PREVIEW PGM", central);
        live_label = new QLabel("\u25CF PGM ATIVO", central);
        live_label->setStyleSheet(
            "color: #36e078; font-size: 16px; font-weight: bold;");

        
monitor_combo = new QComboBox(central);
monitor_combo->setMinimumWidth(180);

for (QScreen *screen : QGuiApplication::screens()) {
    monitor_combo->addItem(
        screen->name() + " (" +
        QString::number(screen->geometry().width()) + "x" +
        QString::number(screen->geometry().height()) + ")");
}

project_button = new QPushButton("Projetar em tela cheia", central);
update_button = new QPushButton("Verificar atualizaÃ§Ãµes", central);
update_button->setVisible(false);

connect(update_button, &QPushButton::clicked, this, [this]() {
    QDesktopServices::openUrl(
        QUrl("https://github.com/dablofilmes/PREVIEW-PGM/releases"));
});
close_fullscreen_button = new QPushButton("Fechar Tela Cheia", central);
close_fullscreen_button->setVisible(false);
header->addWidget(title);
header->addStretch();
header->addWidget(monitor_combo);
header->addWidget(update_button);
header->addWidget(project_button);
header->addWidget(close_fullscreen_button);
header->addWidget(live_label);

connect(project_button, &QPushButton::clicked,
        this, [this]() {
    projetarTelaCheia();
});

connect(close_fullscreen_button, &QPushButton::clicked,
        this, [this]() {
    restaurarJanela();
});

        video = new PgmVideoWidget(central);


source_label = new QLabel("FONTE DE ÁUDIO", central);

audio_source_combo = new QComboBox(central);
audio_source_combo->setMinimumWidth(260);

refresh_sources_button = new QPushButton("Atualizar fontes", central);

auto *sourceLayout = new QHBoxLayout();
sourceLayout->addWidget(source_label);
sourceLayout->addWidget(audio_source_combo, 1);
sourceLayout->addWidget(refresh_sources_button);

        audio_meter = new PgmAudioMeter(central);


        layout->addLayout(header);
        layout->addWidget(video, 1);
layout->addLayout(sourceLayout);
layout->addWidget(audio_meter);

        
setCentralWidget(central);

connect(
    audio_source_combo,
    QOverload<int>::of(&QComboBox::currentIndexChanged),
    this,
    [this](int) {
        conectar_audio();
    });

connect(
    refresh_sources_button,
    &QPushButton::clicked,
    this,
    [this]() {
        atualizar_fontes_audio();
    });

atualizar_fontes_audio();

// MantÃ©m o indicador coerente com o estado da transmissÃ£o.
auto *timer = new QTimer(this);
connect(timer, &QTimer::timeout, this, [this]() {
    const bool streaming = obs_frontend_streaming_active();

    live_label->setText(
        streaming ? "\u25CF AO VIVO" : "\u25CF PGM ATIVO");

    live_label->setStyleSheet(
        streaming
            ? "color: #ff4545; font-size: 16px;"
              "font-weight: bold;"
            : "color: #36e078; font-size: 16px;"
              "font-weight: bold;");
});
timer->start(500);

configurarAtalhoEsc();
        verificarAtualizacao();
}

    
~PreviewPgmWindow() override
{
    desconectar_audio();
}

private:
    QLabel *live_label = nullptr;
    PgmAudioMeter *audio_meter = nullptr;
    PgmVideoWidget *video = nullptr;
    QLabel *source_label = nullptr;
    QPushButton *project_button = nullptr;
    QPushButton *update_button = nullptr;
    QString latest_release_url;
    QPushButton *close_fullscreen_button = nullptr;
    QPushButton *refresh_sources_button = nullptr;

    QComboBox *audio_source_combo = nullptr;
    obs_volmeter_t *audio_meter_handle = nullptr;
    QString selected_audio_source;

static bool listar_fonte_audio(void *param, obs_source_t *source)
{
    auto *names = static_cast<QStringList *>(param);

    if (!(obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO))
        return true;

    const char *name = obs_source_get_name(source);

    if (name && *name)
        names->append(QString::fromUtf8(name));

    return true;
}


QComboBox *monitor_combo = nullptr;
QRect previousGeometry;
bool projecting = false;
bool wasMaximized = false;


void verificarAtualizacao()
{
    if (!update_button)
        return;

    HINTERNET session = WinHttpOpen(
        L"PREVIEW-PGM-Update-Checker",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0);

    if (!session) {
        blog(LOG_WARNING, "[PREVIEW PGM] Falha ao iniciar WinHTTP.");
        return;
    }

    WinHttpSetTimeouts(session, 3000, 3000, 3000, 3000);

    HINTERNET connection = WinHttpConnect(
        session, L"api.github.com",
        INTERNET_DEFAULT_HTTPS_PORT, 0);

    HINTERNET request = connection
        ? WinHttpOpenRequest(
              connection, L"GET",
              L"/repos/dablofilmes/PREVIEW-PGM/releases/latest",
              nullptr, WINHTTP_NO_REFERER,
              WINHTTP_DEFAULT_ACCEPT_TYPES,
              WINHTTP_FLAG_SECURE)
        : nullptr;

    QByteArray response;

    if (request) {
        const wchar_t *headers =
            L"Accept: application/vnd.github+json\r\n"
            L"User-Agent: PREVIEW-PGM-Update-Checker\r\n";

        BOOL ok = WinHttpAddRequestHeaders(
            request, headers, static_cast<DWORD>(-1),
            WINHTTP_ADDREQ_FLAG_ADD);

        if (ok)
            ok = WinHttpSendRequest(
                request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                WINHTTP_NO_REQUEST_DATA, 0, 0, 0);

        if (ok)
            ok = WinHttpReceiveResponse(request, nullptr);

        DWORD status = 0;
        DWORD statusSize = sizeof(status);

        if (ok) {
            ok = WinHttpQueryHeaders(
                request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &status, &statusSize,
                WINHTTP_NO_HEADER_INDEX);
        }

        if (ok && status == 200) {
            DWORD available = 0;

            while (WinHttpQueryDataAvailable(request, &available)
                   && available > 0) {
                QByteArray chunk(static_cast<qsizetype>(available), '\0');
                DWORD downloaded = 0;

                if (!WinHttpReadData(
                        request, chunk.data(), available, &downloaded))
                    break;

                if (downloaded == 0)
                    break;

                chunk.resize(static_cast<qsizetype>(downloaded));
                response.append(chunk);
            }
        } else {
            blog(LOG_WARNING,
                 "[PREVIEW PGM] Falha na consulta ao GitHub. HTTP: %lu",
                 static_cast<unsigned long>(status));
        }

        WinHttpCloseHandle(request);
    } else {
        blog(LOG_WARNING, "[PREVIEW PGM] Falha ao criar requisiÃ§Ã£o WinHTTP.");
    }

    if (connection)
        WinHttpCloseHandle(connection);

    WinHttpCloseHandle(session);

    if (response.isEmpty()) {
        blog(LOG_WARNING, "[PREVIEW PGM] Resposta vazia ao verificar atualizaÃ§Ãµes.");
        return;
    }

    QJsonParseError parseError{};
    const QJsonDocument document =
        QJsonDocument::fromJson(response, &parseError);

    if (parseError.error != QJsonParseError::NoError ||
        !document.isObject()) {
        blog(LOG_WARNING, "[PREVIEW PGM] Resposta invÃ¡lida do GitHub.");
        return;
    }

    const QJsonObject release = document.object();
    const QString latestTag =
        release.value("tag_name").toString().trimmed();
    const QString releaseUrl =
        release.value("html_url").toString();

    if (latestTag.isEmpty())
        return;

    QString currentText = QString::fromUtf8(PLUGIN_VERSION).trimmed();
    QString latestText = latestTag;

    if (currentText.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        currentText.remove(0, 1);

    if (latestText.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
        latestText.remove(0, 1);

    const QVersionNumber currentVersion =
        QVersionNumber::fromString(currentText);
    const QVersionNumber latestVersion =
        QVersionNumber::fromString(latestText);

    if (currentVersion.isNull() || latestVersion.isNull()) {
        blog(LOG_WARNING,
             "[PREVIEW PGM] NÃ£o foi possÃ­vel comparar versÃµes.");
        return;
    }

    if (QVersionNumber::compare(latestVersion, currentVersion) > 0) {
        latest_release_url = releaseUrl;

        update_button->setText(
            QString("Atualizar (%1 disponÃ­vel)").arg(latestTag));

        update_button->setToolTip(
            QString("Sua versÃ£o: %1 | Nova versÃ£o: %2")
                .arg(QString::fromUtf8(PLUGIN_VERSION), latestTag));

        update_button->setVisible(true);

        blog(LOG_INFO, "[PREVIEW PGM] Nova versÃ£o disponÃ­vel: %s",
             latestTag.toUtf8().constData());
    } else {
        blog(LOG_INFO, "[PREVIEW PGM] Plugin atualizado. VersÃ£o atual: %s",
             currentText.toUtf8().constData());
    }
}


void configurarAtalhoEsc()
{
    auto *escapeShortcut =
        new QShortcut(QKeySequence(Qt::Key_Escape), this);

    escapeShortcut->setContext(Qt::ApplicationShortcut);

    connect(escapeShortcut, &QShortcut::activated,
            this, [this]() {
        if (projecting)
            restaurarJanela();
    });
}

void projetarTelaCheia()
{
    QScreen *screen =
        QGuiApplication::screens().value(
            monitor_combo->currentIndex(), nullptr);

    if (!screen || projecting)
        return;

    previousGeometry = geometry();
    wasMaximized = isMaximized();

    projecting = true;

    // No modo de projeÃ§Ã£o, mantÃ©m somente tÃ­tulo, status, vÃ­deo,
    // medidores e o botÃ£o para sair da tela cheia.
    monitor_combo->hide();
    project_button->hide();
    source_label->hide();
    audio_source_combo->hide();
    refresh_sources_button->hide();
    close_fullscreen_button->show();

    // Move a projeÃ§Ã£o para o monitor escolhido na janela de controle.
    showNormal();
    setGeometry(screen->geometry());
    showFullScreen();
}

void restaurarJanela()
{
    if (!projecting)
        return;

    projecting = false;

    close_fullscreen_button->hide();
    monitor_combo->show();
    project_button->show();
    source_label->show();
    audio_source_combo->show();
    refresh_sources_button->show();

    showNormal();
    setGeometry(previousGeometry);

    if (wasMaximized)
        showMaximized();
}


void atualizar_fontes_audio()
{
    if (!audio_source_combo)
        return;

    const QString previous = audio_source_combo->currentText();

    QStringList names;
    obs_enum_sources(listar_fonte_audio, &names);
    names.removeDuplicates();
    names.sort(Qt::CaseInsensitive);

    audio_source_combo->blockSignals(true);
    audio_source_combo->clear();
    audio_source_combo->addItem("Nenhuma fonte");

    for (const QString &name : names)
        audio_source_combo->addItem(name);

    int index = audio_source_combo->findText(previous);

    if (index < 0)
        index = 0;

    audio_source_combo->setCurrentIndex(index);
    audio_source_combo->blockSignals(false);

    conectar_audio();
}

void desconectar_audio()
{
    if (audio_meter_handle) {
        obs_volmeter_remove_callback(
            audio_meter_handle, audio_updated, this);

        obs_volmeter_detach_source(audio_meter_handle);
        obs_volmeter_destroy(audio_meter_handle);
        audio_meter_handle = nullptr;
    }

    selected_audio_source.clear();

    if (audio_meter) {
        audio_meter->setLevels(-60.0f, -60.0f,
                               -60.0f, -60.0f);
    }
}

static bool conectar_fonte_selecionada(
    void *param, obs_source_t *source)
{
    auto *self = static_cast<PreviewPgmWindow *>(param);

    if (!self || self->selected_audio_source.isEmpty())
        return true;

    if (!(obs_source_get_output_flags(source) & OBS_SOURCE_AUDIO))
        return true;

    const char *name = obs_source_get_name(source);

    if (!name ||
        self->selected_audio_source != QString::fromUtf8(name))
        return true;

    auto *meter = obs_volmeter_create(OBS_FADER_LOG);

    if (!meter)
        return true;

    obs_volmeter_add_callback(meter, audio_updated, self);

    if (obs_volmeter_attach_source(meter, source)) {
        self->audio_meter_handle = meter;
        return false;
    }

    obs_volmeter_remove_callback(meter, audio_updated, self);
    obs_volmeter_destroy(meter);

    return false;
}

void conectar_audio()
{
    desconectar_audio();

    if (!audio_source_combo)
        return;

    const QString selected = audio_source_combo->currentText();

    if (selected.isEmpty() || selected == "Nenhuma fonte") {
        blog(LOG_INFO,
             "[PREVIEW PGM] MediÃ§Ã£o de Ã¡udio desativada.");
        return;
    }

    selected_audio_source = selected;

    obs_enum_sources(conectar_fonte_selecionada, this);

    if (!audio_meter_handle) {
        blog(LOG_WARNING,
             "[PREVIEW PGM] NÃ£o foi possÃ­vel conectar a fonte: %s",
             selected.toUtf8().constData());
    } else {
        blog(LOG_INFO,
             "[PREVIEW PGM] Medindo a fonte: %s",
             selected.toUtf8().constData());
    }
}


static void audio_updated(
    void *param,
    const float magnitude[MAX_AUDIO_CHANNELS],
    const float peak[MAX_AUDIO_CHANNELS],
    const float input_peak[MAX_AUDIO_CHANNELS])
{
    Q_UNUSED(magnitude);
    Q_UNUSED(input_peak);

    auto *self = static_cast<PreviewPgmWindow *>(param);

    if (!self || !self->audio_meter)
        return;

    // Barra colorida: acompanha o pico, como o preenchimento
    // principal do Mixer oficial do OBS.
    const float leftDb = std::clamp(
        peak[0], -60.0f, 0.0f);

    const float rightDb = std::clamp(
        peak[1], -60.0f, 0.0f);

    // Marcadores brancos: acompanham os mesmos picos.
    const float peakLeft = leftDb;
    const float peakRight = rightDb;

    QPointer<PgmAudioMeter> meter(self->audio_meter);

    QMetaObject::invokeMethod(
        self->audio_meter,
        [meter, leftDb, rightDb, peakLeft, peakRight]() {
            if (meter) {
                meter->setLevels(
                    leftDb, rightDb,
                    peakLeft, peakRight);
            }
        },
        Qt::QueuedConnection);
}


};

static QPointer<PreviewPgmWindow> preview_window;

static void abrir_preview_pgm(void *data)
{
    Q_UNUSED(data);

    if (preview_window) {
        preview_window->show();
        preview_window->raise();
        preview_window->activateWindow();
        return;
    }

    QWidget *parent = static_cast<QWidget *>(
        obs_frontend_get_main_window());

    preview_window = new PreviewPgmWindow(parent);
    preview_window->setAttribute(Qt::WA_DeleteOnClose);
    preview_window->show();
}

bool obs_module_load(void)
{
    blog(LOG_INFO,
         "[PREVIEW PGM] Plugin carregado - versÃ£o %s",
         PLUGIN_VERSION);

    obs_frontend_add_tools_menu_item(
        "Abrir PREVIEW PGM",
        abrir_preview_pgm,
        nullptr);

    return true;
}

void obs_module_unload(void)
{
    if (preview_window) {
        preview_window->close();
        preview_window = nullptr;
    }

    blog(LOG_INFO, "[PREVIEW PGM] Plugin descarregado.");
}
