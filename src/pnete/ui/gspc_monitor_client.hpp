// This file is part of GPI-Space.
// Copyright (C) 2020 Fraunhofer ITWM
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program. If not, see <https://www.gnu.org/licenses/>.

#pragma once

#include <fhg/util/parse/position.hpp>

#include <boost/optional.hpp>

#include <QMap>
#include <QMutex>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTcpSocket>
#include <QTimer>

#include <functional>

namespace fhg
{
  namespace pnete
  {
    namespace ui
    {
      class monitor_client : public QObject
      {
        Q_OBJECT

      public:
        monitor_client (const QString& host, int port, QObject* parent = nullptr);

        void request_action ( const QStringList&
                            , const QString&
                            , const QMap<QString, std::function<QString()>>&
                            );
        void request_layout_hint (const QString&);
        void request_action_description (const QStringList&);
        void request_hostlist();
        void request_possible_status();
        void request_status (const QSet<QString>);

        void pause();
        void resume();

        enum action_result_code
        {
          okay,
          fail,
          warn,
        };

        struct action_argument_data
        {
          enum type
          {
            boolean,
            directory,
            duration,
            filename,
            integer,
            string,
          };

          action_argument_data (const QString& name);

          QString _name;
          boost::optional<type> _type;
          boost::optional<QString> _label;
          boost::optional<QString> _default;

          void append (fhg::util::parse::position&);
        };

      signals:
        void action_result ( const QString&
                           , const QString&
                           , const action_result_code&
                           , const boost::optional<QString>&
                           , QList<QPair<QString, QString>>
                           );
        void nodes (QStringList);
        void nodes_details (const QString&, const boost::optional<QString>&);
        void nodes_state (const QString&, const boost::optional<QString>&);
        void states_actions_long_text (const QString&, const QString&);
        void states_actions_requires_confirmation (const QString&, bool);
        void states_actions_arguments
          (const QString&, const QList<action_argument_data>&);
        void states_actions_expected_next_state (const QString&, const QString&);
        void states_add (const QString&, const QStringList&);
        void states_layout_hint_border (const QString&, const QColor&);
        void states_layout_hint_character (const QString&, const char&);
        void states_layout_hint_color (const QString&, const QColor&);
        void states_layout_hint_hidden (const QString&, const bool&);
        void states_layout_hint_descriptive_name (const QString&, const QString&);

      private slots:
        void check_for_incoming_messages();
        void send_outstanding();
        void may_read();

      private:
        void possible_status (fhg::util::parse::position&);
        void action_description (fhg::util::parse::position&, const QString&);
        void layout_hint (fhg::util::parse::position&, const QString&);
        void status_update (fhg::util::parse::position&);
        void action_result (fhg::util::parse::position&);

        void push (const QString& message);

        typedef QList<QString> messages_type;
        messages_type _outstanding_incoming;
        mutable QMutex _outstanding_incoming_lock;

        messages_type _outstanding_outgoing;
        mutable QMutex _outstanding_outgoing_lock;

        QTcpSocket _socket;

        QTimer* _timer;
      };
    }
  }
}
