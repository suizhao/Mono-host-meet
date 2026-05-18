#pragma once



#include "domain/services/backend_service.h"

#include "domain/services/room_service.h"



#include <QObject>

#include <QString>



class HomeController : public QObject {

  Q_OBJECT

  Q_PROPERTY(QString statusMessage READ statusMessage NOTIFY statusMessageChanged)

  Q_PROPERTY(bool joinInProgress READ joinInProgress NOTIFY joinInProgressChanged)

  Q_PROPERTY(bool inRoom READ inRoom NOTIFY inRoomChanged)

  Q_PROPERTY(bool hasErrorBanner READ hasErrorBanner NOTIFY errorBannerChanged)

  Q_PROPERTY(QString errorBannerText READ errorBannerText NOTIFY errorBannerChanged)

  Q_PROPERTY(bool recordingActive READ recordingActive NOTIFY recordingActiveChanged)

  Q_PROPERTY(bool recordingRequestInFlight READ recordingRequestInFlight NOTIFY recordingRequestInFlightChanged)

  Q_PROPERTY(QString recordingStatusText READ recordingStatusText NOTIFY recordingStatusTextChanged)
  Q_PROPERTY(QString currentRoomName READ currentRoomName NOTIFY currentRoomNameChanged)
  Q_PROPERTY(bool isHost READ isHost NOTIFY isHostChanged)
  Q_PROPERTY(bool allMuted READ allMuted NOTIFY allMutedChanged)
  Q_PROPERTY(bool prejoinVisible READ prejoinVisible NOTIFY prejoinVisibleChanged)
  Q_PROPERTY(QString defaultParticipantName READ defaultParticipantName NOTIFY defaultParticipantNameChanged)



public:

  HomeController(IBackendService* backendService, IRoomService* roomService,

                 QObject* parent = nullptr);



  QString statusMessage() const;

  bool joinInProgress() const;

  bool inRoom() const;

  bool hasErrorBanner() const;

  QString errorBannerText() const;

  bool recordingActive() const;

  bool recordingRequestInFlight() const;
  QString recordingStatusText() const;
  QString currentRoomName() const;
  bool isHost() const;
  bool prejoinVisible() const;
  QString defaultParticipantName() const;

  Q_INVOKABLE void startDemoMeeting();
  Q_INVOKABLE void openCustomMeeting();
  Q_INVOKABLE void confirmCustomJoin(const QString& roomName,
                                     const QString& participantName);
  Q_INVOKABLE void cancelCustomJoin();
  Q_INVOKABLE void leaveRoom();
  Q_INVOKABLE void startRecording(const QVariantList& audioSids = {},
                                   const QVariantList& videoSids = {});
  Q_INVOKABLE void stopRecording();
  Q_INVOKABLE void disbandRoom();
  Q_INVOKABLE void toggleMuteAll();
  bool allMuted() const;



signals:

  void statusMessageChanged();

  void joinInProgressChanged();

  void inRoomChanged();

  void errorBannerChanged();

  void sessionResetRequested();

  void recordingActiveChanged();

  void recordingRequestInFlightChanged();

  void recordingStatusTextChanged();
  void currentRoomNameChanged();
  void isHostChanged();
  void allMutedChanged();
  void prejoinVisibleChanged();
  void defaultParticipantNameChanged();



private:

  void setStatusMessage(QString message);

  void setJoinInProgress(bool value);

  void setInRoom(bool value);

  void setErrorBannerText(QString message);
  void clearErrorBanner();
  void setCurrentRoomName(QString name);
  void setPrejoinVisible(bool value);
  void onRoomDisbanded(const QString& roomName);
  void onMuteAllCompleted(const QString& roomName, int mutedCount);

  QString buildUserFacingError(const QString& operation, int statusCode,

                               const QString& message) const;



  IBackendService* m_backendService;

  IRoomService* m_roomService;

  QString m_statusMessage;

  QString m_lastRoomName;

  QString m_lastParticipantName;

  bool m_joinInProgress = false;

  bool m_inRoom = false;

  QString m_errorBannerText;

  bool m_recordingActive = false;

  QString m_recordingEgressId;
  bool m_recordingRequestInFlight = false;

  bool m_isHost = false;
  bool m_allMuted = false;
  QString m_recordingStatusText = "录制状态：未开始";
  QString m_currentRoomName;
  bool m_prejoinVisible = false;

};

