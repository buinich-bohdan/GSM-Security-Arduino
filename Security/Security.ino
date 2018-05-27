#define DEBUG                                     // Для режима отладки нужно раскомментировать эту строку
#ifdef DEBUG // В случае существования DEBUG 
#define DEBUG_PRINT(x) Serial.print (x) // Создаем "переадресацию" на стандартную функцию 
#define DEBUG_PRINTLN(x)  Serial.println (x)
#else // Если DEBUG не существует - игнорируем упоминания функций 
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

#include <SoftwareSerial.h> // Библиотека програмной реализации обмена по UART-протоколу
SoftwareSerial SIM800(8, 9); // RX, TX

unsigned long lastUpdate = 0; // Время последнего обновления
long updatePeriod = 90000; // Проверять каждых 90 сек

int sensor1 = 0;
int flag1 = 0;

String tasks[10]; // Переменная для хранения списка задач к исполнению
bool executingTask = false; // Флаг исполнения отложенной задачи
float balance = 0.0; // Переменная для хранения данных о балансе SIM-карты

String stat_sec = "Snyato s ohrani!";
String phones = "+380XXXXXXXXX"; // Белый список телефонов
String n = "+380XXXXXXXXX";
char smsDv[] = "Wnimaniye! Dvijeniye na objekte!";

void setup() {

  Serial.begin(9600);  //Скорость порта для связи Arduino с компьютером
  SIM800.begin(9600);  //Скорость порта для связи Arduino с GSM модулем
  delay(5000); // Инстализация модема

  DEBUG_PRINTLN("Start!");
  pinMode(12, INPUT_PULLUP);
  sendATCommand("AT", true); // Отправили AT для настройки скорости обмена данными
  // Команды настройки модема при каждом запуске
  sendATCommand("AT+CLIP=1", true);             // Включаем АОН
  sendATCommand("AT+CMGF=1", true); // Включаем текстовый режима SMS (Text mode) и сразу сохраняем значение (AT&W)!
  sendATCommand("AT+CMGDA=\"DEL ALL\"", true); // Удаляем все сообщения, чтобы не забивали память МК
  addTask("getBalance"); // Добавляем задачу - "Запрос баланса"
  //Добавляем задачу - "Отправить SMS админу о включении устройства"
  //addTask("sendSMS;" + Admin + ";Init - OK.\r\nStatus: System Online");

  lastUpdate = millis() + 10000; // Обнуляем таймер
}

String sendATCommand(String cmd, bool waiting) {  // Функция отправки AT-команды
  String _response = "";
  DEBUG_PRINTLN(cmd);                             // Дублируем в Serial отправляемую команду
  SIM800.println(cmd);                            // Отправляем команду модулю
  if (waiting) {                                  // Если нужно ждать ответ от модема...
    _response = waitResponse();                   // Результат ответа сохраняем в переменную
    if (_response.startsWith(cmd)) {              // Если ответ начинается с отправленной команды, убираем её, чтобы не дублировать
      _response = _response.substring(_response.indexOf("\r\n", cmd.length()) + 2);
    }
    DEBUG_PRINTLN(_response);                     // Выводим ответ в Serial
    return _response;                             // Возвращаем ответ
  }
  return "";                                      // Еси ждать ответа не нужно, возвращаем пустую строку
}

String waitResponse() {                           // Функция ожидания ответа от GSM-модуля
  String _buffer;                                 // Переменная для хранения ответа
  long _timeout = millis() + 10000;               // Таймаут наступит через 10 секунд

  while (!SIM800.available() && millis() < _timeout)  {}; //Ждем...
  if (SIM800.available()) {                       // Если есть что принимать...
    _buffer = SIM800.readString();                // ...принимаем
    //DEBUG_PRINTLN("Ok - response");
    return _buffer;                               // и возвращаем полученные данные
  }
  else {                                          // Если таймаут вышел...
    //DEBUG_PRINTLN("Timeout...");
  }
  return "";                                      // и возвращаем пустую строку
}

bool hasmsg = false; // Флаг наличия сообщений к удалению
void loop() {
  String _buffer = "";                            // Переменная хранения ответов от GSM-модуля
  if (millis() > lastUpdate && !executingTask) {  // Цикл автоматической проверки SMS, повторяется каждые updatePeriod (90 сек)
    do {
      _buffer = sendATCommand("AT+CMGL=\"REC UNREAD\",1", true);  // Отправляем запрос чтения непрочитанных сообщений
      if (_buffer.indexOf("+CMGL: ") > -1) {                      // Если есть хоть одно, получаем его индекс
        int msgIndex = _buffer.substring(_buffer.indexOf("+CMGL: ") + 7, _buffer.indexOf("\"REC UNREAD\"", _buffer.indexOf("+CMGL: "))).toInt();
        char i = 0;                                               // Объявляем счетчик попыток
        do {
          i++;                                                    // Увеличиваем счетчик
          _buffer = sendATCommand("AT+CMGR=" + (String)msgIndex + ",1", true);  // Пробуем получить текст SMS по индексу
          _buffer.trim();

          // Убираем пробелы в начале/конце
          if (_buffer.endsWith("OK")) {                           // Если ответ заканчивается на "ОК"
            getActionBySMS(_buffer);                              // Отправляем текст сообщения на обработку
            if (!hasmsg) hasmsg = true;                           // Ставим флаг наличия сообщений для удаления
            sendATCommand("AT+CMGR=" + (String)msgIndex, true);   // Делаем сообщение прочитанным
            break;                                                // Выход из do{}
          }
          else {                                                  // Если сообщение не заканчивается на OK
            //Serial.println ("Error answer");
          }
          sendATCommand("\n", true);                              // Перестраховка - вывод новой строки
        } while (i < 10);                                         // Пока попыток меньше 10
        break;
        sendATCommand("AT+CMGDA=\"DEL ALL\"", true); // Удаляем все сообщения, чтобы не забивали память МК

      }
      else {
        lastUpdate = millis() + updatePeriod;                     // Если все обработано, обновляем время последнего обновления
        if (hasmsg) {                                             // Если были сообщения для обработки
          addTask("clearSMS");                                    // Добавляем задание для удаления сообщений
          addTask("getBalance"); // Добавляем задачу - "Запрос баланса"

          hasmsg = false;                                         // Сбрасываем флаг наличия сообщений
        }
        break;                                                    // Выходим из цикла
      }
    } while (1);
  }
  if (millis() > lastUpdate + 180000 && executingTask) { // Таймаут на выполнение задачи - 3 минуты
    //DEBUG_PRINTLN("ExTask-true!");
    sendATCommand("\n", true);
    executingTask = false; // Если флаг не был сброшен по исполению задачи, сбрасываем его принудительно через 3 минуты
  }

  if ((digitalRead(12) == HIGH) && sensor1 == 1 && flag1 == 0) { //Первое срабатывание датчика движения
    addTask(getSendSMSTaskString(n, smsDv)); // Посылаем СМС о том, что есть движение
    flag1++;
    delay(5000); // Ждем 5 секунд
  }
  if (flag1 >= 1) { //Повторное срабатывание датчика движения

    tone(10, 2780, 200); // Сирена
  }

  if (SIM800.available()) { // Если модем, что-то отправил...
    String msg = waitResponse(); // Получаем ответ от модема для анализа
    msg.trim(); // Убираем лишние пробелы в начале и конце
    DEBUG_PRINTLN(".. " + msg); // Если нужно выводим в монитор порта

    if (msg.startsWith("+CUSD:")) { // Если USSD-ответ о балансе
      String msgBalance = msg.substring(msg.indexOf("\"") + 2); // Парсим ответ
      msgBalance = msgBalance.substring(0, msgBalance.indexOf("\n"));
      delay(500);
      //DEBUG_PRINTLN(msgBalance);
      balance = getDigitsFromString((String)msgBalance); // Сохраняем баланс
      deleteFirstTask(); // Удаляем задачу
      executingTask = false; // Сбрасываем флаг исполнения
      DEBUG_PRINTLN("Balance: " + (String) balance + " .grn"); // Отчитываемся в Serial
    } else if (msg.startsWith("+CMGS:")) { // Результат отправки сообщения
      deleteFirstTask(); // Удаляем задачу
      executingTask = false; // Сбрасываем флаг исполнения
      DEBUG_PRINTLN("SMS sending - task removed."); // Отчитываемся в Serial
      //addTask("getBalance"); // Добавляем задачу запроса баланса
    } else if (msg.startsWith("RING")) { // При входящем вызове
      sendATCommand("ATH", true); // Всегда сбрасываем
    } else if (msg.startsWith("+CMTI:")) { // Незапрашиваемый ответ - приход сообщения
      lastUpdate = millis(); // Сбрасываем таймер автопроверки наличия сообщений
    } else if (msg.startsWith("ERROR")) { // Ошибка исполнения команды
      DEBUG_PRINTLN("Error executing last command.");
      executingTask = false; // Сбрасываем флаг исполнения, но задачу не удаляем - на повторное исполнение
    }
  }

  if (!executingTask && tasks[0] != "") { // Если никакая задача не исполняется, и список задач не пуст, то запускаем выполнение.
    showAllTasks(); // Показать список задач

    String task = tasks[0];
    if (tasks[0].startsWith("sendSMS")) { // Если задача - отправка SMS - отправляем
      task = task.substring(task.indexOf(";") + 1);
      executingTask = true; // Флаг исполнения в true
      sendSMS(task.substring(0, task.indexOf(";")),
              task.substring(task.indexOf(";") + 1));

    } else if ((tasks[0].startsWith("getBalance"))) { // Задача - запрос баланса
      executingTask = true; // Флаг исполнения в true
      sendATCommand("AT+CUSD=1,\"*101#\"", true); // Отправка запроса баланса
    } else if ((tasks[0].startsWith("clearSMS"))) { // Задача - удалить все прочитанные SMS
      sendATCommand("AT+CMGDA=\"DEL READ\"", true); // Флаг исполнения не устанавливаем - здесь не нужно.
      deleteFirstTask(); // Удаляем задачу, сразу после исполнения
    } else {
      DEBUG_PRINTLN("Error: unknown task - " + task);
    }
  }
}

void getActionBySMS(String msg) { // Парсим SMS
  String msgheader = "";
  String msgbody = "";
  String msgphone = "";

  msg = msg.substring(msg.indexOf("+CMGR: "));
  msgheader = msg.substring(0, msg.indexOf("\r")); // Выдергиваем телефон

  msgbody = msg.substring(msgheader.length() + 2);
  msgbody = msgbody.substring(0, msgbody.lastIndexOf("OK")); // Выдергиваем текст SMS
  msgbody.trim();

  int firstIndex = msgheader.indexOf("\",\"") + 3;
  int secondIndex = msgheader.indexOf("\",\"", firstIndex);
  msgphone = msgheader.substring(firstIndex, secondIndex);

  DEBUG_PRINTLN("Phone: " + msgphone); // Выводим номер телефона
  DEBUG_PRINTLN("Message: " + msgbody); // Выводим текст SMS

  if (msgphone.length() > 10 && phones.indexOf(msgphone) > -1) { // Если телефон в белом списке, то...
    String result = ""; // Обрабатываем это сообщение
    if (msgbody.startsWith("Status")) { // Команда запроса статуса
      DEBUG_PRINTLN("Status: " + String(stat_sec));
      addTask(getSendSMSTaskString(msgphone, String(stat_sec))); // Добавляем задачу об отправке SMS со статусом
      //addTask("getBalance"); // Добавляем задачу о запросе баланса
      showAllTasks(); // Выводим все задачи
    } else if (msgbody.startsWith("Balance")) { // Команда запроса баланса
      addTask(getSendSMSTaskString(msgphone, "Balance: " + String(balance) + " grn.")); // Добавляем задачу об отправке SMS с балансом
      //addTask("getBalance"); // Добавляем задачу о запросе баланса
      showAllTasks(); // Выводим все задачи
    } else if (msgbody.startsWith("Callme")) { // Команда осуществить исходящий вызов
      sendATCommand("ATD" + msgphone + ";", true);
    } else if (msgbody.startsWith("Help")) { // Команда получения помощи по командам
      addTask(getSendSMSTaskString(msgphone, getHelpSMS()));
      //addTask("getBalance");
      showAllTasks();
    } else if (msgbody.startsWith("checknow")) { // Обнулить таймер периодической проверки - проверить сразу
      lastUpdate = millis();
    } else if (msgbody.startsWith("1")) {
      //Serial.print("Postanovka na ohranu!"); // Команда "1" - Поставить на охрану
      sensor1 = 1;
      flag1 = 0;
      stat_sec = "Na ohrane";
      addTask(getSendSMSTaskString(msgphone, "Security: ON")); // Добавляем задачу об отправке SMS со статусом
      //addTask("getBalance"); // Добавляем задачу о запросе баланса
      showAllTasks(); // Выводим все задачи
    } else if (msgbody.startsWith("0")) {
      DEBUG_PRINTLN("Snyato s ohrani!"); // Команда "0" - снять с охраны
      sensor1 = 0;
      flag1 = 0;
      stat_sec = "Snyato s ohrani!";
      addTask(getSendSMSTaskString(msgphone, "Security: OFF")); // Добавляем задачу об отправке SMS со статусом
      //addTask("getBalance"); // Добавляем задачу о запросе баланса
      showAllTasks(); // Выводим все задачи
    }
  } else {
    DEBUG_PRINTLN("Unknown phonenumber");
  }

}

String getHelpSMS() { // Текст сообщения с помощью по камандам
  return "Security[0 or 1], Status, Balance, Callme";
}
String getSendSMSTaskString(String phone, String msg) { // Формируем строку задачи отправки SMS
  return "sendSMS;" + phone + ";" + msg;
}

// =================================== Tasks =========================================
void showAllTasks() { // Показать все задачи
  //DEBUG_PRINTLN("All Tasks:");
  for (int i = 0; i < 10; i++) {
    if (tasks[i] == "") break;
    DEBUG_PRINTLN("Task " + (String)(i + 1) + ": " + tasks[i]);
  }
}

void deleteFirstTask() { // Удалить первую задачу, остальные передвинуть вверх на 1
  for (int i = 0; i < 10 - 1; i++) {
    tasks[i] = tasks[i + 1];
    if (tasks[i + 1] == "") break;
  }
}

void addTask(String task) { // Добавить задачу в конец очереди
  for (int i = 0; i < 10; i++) {
    if (tasks[i] == task && (task == "clearSMS" || task == "getBalance")) {
      //DEBUG_PRINTLN("Task already exists " + (String)(i + 1) + ": " + task);
      return;
    }
    if (tasks[i] == "") {
      tasks[i] = task;
      DEBUG_PRINTLN("Task " + (String)(i + 1) + " added: " + task);
      return;
    }
  }
  //DEBUG_PRINTLN("Error!!! Task NOT added: " + task);
}

float getDigitsFromString(String str) { // Функция выбора цифр из сообщения - для парсинга баланса из USSD-запроса
  bool flag = false;
  String digits = "0123456789.";
  String result = "";

  for (int i = 0; i < 25; i++) {
    char symbol = char(str.substring(i, i + 1)[0]);
    if (digits.indexOf(symbol) > -1) {
      result += symbol;
      if (!flag) flag = true;
    } else {
      if (flag) break;
    }
  }
  double(balance) = result.toFloat();
  return balance;
}

void sendSMS(String phone, String message) {
  sendATCommand("AT+CMGS=\"" + phone + "\"", true); // Переходим в режим ввода текстового сообщения
  sendATCommand(message + "\r\n" + (String)((char) 26), true); // После текста отправляем перенос строки и Ctrl+Z
}

