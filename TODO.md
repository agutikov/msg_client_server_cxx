
// чтобы не растягивать обсуждение буду принимать какие-то решения по неоднозначным вопросам сам и обосновывать почему


// Реализовать небольшое клиент-серверное приложение

// Клиент коннектится к серверу.
// Сервер выдает клиенту ID на основе sha256(client_IP + client_PORT + TIME)

// Клиент с переодичности в 5 секунд посылает на сервер сообщение со своими (случайно сгенерированными) данными MSG и с уникальным идентификатором MSG_ID = sha256(IP + PORT + TIME + MSG)

?? чей IP и порт? сервера куда коннектится клиент или самого клиента? IP "с котрого клиент отправляет сообщение" это очень размытая штука, особенно если клиент не биндится к IP на интерфейсе или находится за NAT
// раз ничего не написано в требованиях про доставку сообщений от клиента к серверу
// а также ввиду того что клиенту, отправившиму сообщение оно уже придти не должно
// а также, особенно, ввиду того что очень желательно чтобы MSG_ID были уникальными глобально
// то до того как сообщение попадет на сервер ему MGS_ID вообще не нужен
// заимплементить присвоение уникального MSG_ID на сервере проще, но
// все-таки чтобы в какой-то степени выполнить требование сделаю присвоение MSG_ID на клиенте = sha256(client_ID + msg_creation_TIME + MSG)
// т.к. client_ID уже содержит информацию о IP+PORT клиента

// Сервер получив новое сообщение c MSG_ID, рассылает новый список MSG_ID всем своим клиентам.
// получается что пока клиент не опубликует свое первое сообщение - он не получит список
// добавлю в спеку "Сервер рассылает список MSG_ID всем новым только что подключившимся клиентам"

// Клиент получив список MSG_ID, запускает таймер на 1 секунду + random (от 250 до 500 ms)
// Через указанное время, клиент забирает случайное MSG_ID из доступного списка, и если сообщение удалось получить, то распечатывает на свой std::cerr
// если не удалось получить - ничего не делает и этот список MSG_ID дропается

// то что пропало из спеки, но по логике осталось актуальным
// сервер удаляет сообщение после доставки (или после отправки) клиенту
// вообще если в спеке "доставлено" то все становится сильно сложнее
// допустим сервер (который хранит сообщения в памяти) не падает, а сами сообщения это что-то важное и не должны теряться после того как попали на сервер
// т.е. если клиет сгенерил сообщение но не смог доставить его на сервер - это его проблемы, если клиент не упал - пусть ретраит до бесконечности и пишет в лог
// при этом сообщение должно быть доставлен только одному клиенту - первому запросившему
// а если первый запросивший не смог получить ?
// сделаем так - если сообщение не было доставлено запросившему его клиенту - то через таймаут оно снова становится доступным для всех (кроме автора)

// Получается "игра", где клиенты получив MSG_ID, пытаются получить опубликованные сообщения.

// Сервер ведет счетчики отправленных и полученных сообщений по клиентам. 
// С переодичности в 30 секунд, сервер выводит на std::cerr статистику

// Общение по собственному протоколу поверх TCP/IP
// Т.к. задача не реальная и в условиях не сказано то думать про расширение протокола не буду
// но все же протокол будет не совсем тупой, а немного более гибкий чем минимальный

// Для автоматизации сериализации структур использовать boost::fusion
// Для работы со временем использовать std::chrono
// Для реализации сетевого взаимодействия использовать boost::asio
// Для реализации потоков использовать возможности boost::asio
// Для реализации таймеров использовать возможности boost::asio
// Для хранения данных использовать boost::multi_index
// Для генерации хешей использовать OpenSSL

// Сборка проекта должна осуществляться с помощью CMake
// Boost версии минимум 1.67
// Проверятся будет на Ubuntu 18.04
// Проект опубликовать на github и прислать на него ссылку.


#############################################################################################


greeter (server -> client)
    client_id

post (client -> server)
    msg_id
    msg_data

post_responce (server -> client)
    msg_id
    msg_status

current_msg_id_list (server -> client)
    msg_id_list[]

get (client -> server) клиент запрашивает сообщение -> сервер его лочит
    msg_id

msg_data (server -> client) сервер шлет сообщение клиенту -> клиент его получает (клиент знает что он владеет сообщением, но сервер еще нет, поэтому клиент еще не присваивает сообщение себе - т.е. не печатает его)
    msg_id
    msg_status
    msg_data (optional)

msg_status (client -> server, server -> client)
    msg_id
    msg_status

по сути надежная доставка сообщения от сервера к клиенту решается с помощью распределенной state-машины
в данном случае все просто - участвуют всего две ноды: сервер и клиент
их задача договориться о том кому принадлежит сообщение
по запросу сервер пытается передать "право собственности" на сообщение клиенту
а в итоге диалог прекращается либо когда обе стороны "договорятся" либо по таймауту
т.е. у каждого локальная информация о состоянии сообщения должна совпадать с тем что приходит от пира
и вот когда она совпадет - сторона прекращает ретраить отправку пакетов со своей стороны
получается что ответственность за корректную передачу собственности на сообщение ложится на логику state-машины
важный момент - обе стороны ведут себя добропорядочно и не пытаются "обмануть" пира, иначе протокол становится еще сложнее

состояния сообщения
для сервера:
    default - доступно всем кроме автора
    claimed + client_id - пошел счетчик ретраев доставки клиенту, если клиент отвалится - возвращается в default
    delivered - конечное состояние на стороне сервера - сервер удаляет сообщение, дальше если клиент его потерял - это его проблемы
для клиента:
    claimed - уже запросил, но еще не пришло, повтори отправку
    delivered - конечное состояние на стороне клиента






слово msg уже занято - значит msg как часть протокола будет называться packet

protocol:
    packet_type_id:
        server:
            greeter
            post_responce
            current_msg_id_list
            get_responce
        client:
            post
            get
    packet_field_id:
        client_id
        msg_id
        msg_data
        msg_status
        msg_id_list


#############################################################################################

dot + graphviz - Диаграмма состояний сообщения на клиенте и на сервере
plantuml - диаграмма взаимодействия clients-server

Сеть петри - представление состояний сообщение во всей системе целиком: сервер + клиент + сеть + сообщение

Обоснование необходимости персистенет стораджа







