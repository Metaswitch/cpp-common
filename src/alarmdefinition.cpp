/**
 * @file alarmdefinition.cpp
 *
 * Project Clearwater - IMS in the Cloud
 * Copyright (C) 2014  Metaswitch Networks Ltd
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or (at your
 * option) any later version, along with the "Special Exception" for use of
 * the program along with SSL, set forth below. This program is distributed
 * in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more
 * details. You should have received a copy of the GNU General Public
 * License along with this program.  If not, see
 * <http://www.gnu.org/licenses/>.
 *
 * The author can be reached by email at clearwater@metaswitch.com or by
 * post at Metaswitch Networks Ltd, 100 Church St, Enfield EN2 6BQ, UK
 *
 * Special Exception
 * Metaswitch Networks Ltd  grants you permission to copy, modify,
 * propagate, and distribute a work formed by combining OpenSSL with The
 * Software, or a work derivative of such a combination, even if such
 * copying, modification, propagation, or distribution would otherwise
 * violate the terms of the GPL. You must comply with the GPL in all
 * respects for all of the code used other than OpenSSL.
 * "OpenSSL" means OpenSSL toolkit software distributed by the OpenSSL
 * Project and licensed under the OpenSSL Licenses, or a work based on such
 * software and licensed under the OpenSSL Licenses.
 * "OpenSSL Licenses" means the OpenSSL License and Original SSLeay License
 * under which the OpenSSL Project distributes the OpenSSL toolkit software,
 * as those licenses appear in the file LICENSE-OPENSSL.
 */

#include "alarmdefinition.h"

// See alarmdefinition.h for information on adding a new alarm. Please keep
// AlarmDefinition's defined here in the same order as AlarmDef::Index.
//
// Note: strings are limited to 255 characters per the MIB definition.

namespace AlarmDef {

  const std::vector<AlarmDefinition> alarm_definitions =
  {
//         1234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890

    {SPROUT_PROCESS_FAIL, SOFTWARE_ERROR,
      {
        {CLEARED,
          "Sprout: Process failure cleared",
          "The Sprout process has been restored to normal operation."
        },
        {CRITICAL,
          "Sprout: Process failure",
          "Monit has detected that the Sprout process has failed. A restart will automatically be attempted. "
          "If this alarm does not clear, the Sprout process may have been stopped or an unrecoverable failure "
          "may have occurred."
        }
      }
    },

    {SPROUT_HOMESTEAD_COMM_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Sprout: Homestead communication error cleared",
          "Sprout communication to at least one Homestead has been restored."
        },
        {CRITICAL,
          "Sprout: Homestead communication error",
          "Sprout is unable to contact any Homesteads. It will periodically attempt to reconnect. If this "
          "alarm does not clear, ensure that at least one Homestead is operational and that network "
          "connectivity exists between it and Sprout."
        }
      }
    },

    {SPROUT_MEMCACHED_COMM_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Sprout: Memcached communication error cleared",
          "Sprout communication to at least one Memcached has been restored."
        },
        {CRITICAL,
          "Sprout: Memcached communication error",
          "Sprout is unable to contact any Memcacheds. It will periodically attempt to reconnect. If this "
          "alarm does not clear, ensure that at least one Memcached is operational and that network "
          "connectivity exists between it and Sprout."
        }
      }
    },

    {SPROUT_REMOTE_MEMCACHED_COMM_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Sprout: Remote Memcached communication error cleared",
          "Sprout communication to at least one remote Memcached has been restored."
        },
        {CRITICAL,
          "Sprout: Remote Memcached communication error",
          "Sprout is unable to contact any remote Memcacheds. It will periodically attempt to reconnect. "
          "If this alarm does not clear, ensure that at least one remote Memcached is operational and "
          "that network connectivity exists between it and Sprout."
        }
      }
    },

    {SPROUT_CHRONOS_COMM_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Sprout: Chronos communication error cleared",
          "Sprout communication to at least one Chronos has been restored."
        },
        {MAJOR,
          "Sprout: Chronos communication error",
          "Sprout is unable to contact any Chronos's. It will periodically attempt to reconnect. If this "
          "alarm does not clear, ensure that at least one Chronos is operational and that network "
          "connectivity exists between it and Sprout."
        }
      }
    },

    {SPROUT_RALF_COMM_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Sprout: Ralf communication error cleared",
          "Sprout communication to at least one Ralf has been restored."
        },
        {MAJOR,
          "Sprout: Ralf communication error",
          "Sprout is unable to contact any Ralfs. It will periodically attempt to reconnect. If this alarm "
          "does not clear, ensure that at least one Ralf is operational and that network connectivity exists "
          "between it and Sprout."
        }
      }
    },

    {SPROUT_ENUM_COMM_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Sprout: ENUM communication error cleared",
          "Sprout communication to the ENUM server has been restored."
        },
        {MAJOR,
          "Sprout: ENUM communication error",
          "Sprout is unable to contact the ENUM server. It will periodically attempt to reconnect. If this "
          "alarm does not clear, ensure that the ENUM server is operational and that network connectivity "
          "exists between it and Sprout."
        }
      }
    },

    {SPROUT_VBUCKET_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Sprout: Memcached vBucket communication error cleared",
          "Sprout communication to Memcached for a piece of data has been restored."
        },
        {MAJOR,
          "Sprout: Memcached vBucket communication error",
          "Sprout is unable to contact any Memcacheds for a piece of data. Some subscriber data will be "
          "unavailable. If this alarm does not clear, ensure that all Memcacheds are operational and that "
          "network connectivity exists between them and Sprout."
        }
      }
    },

    {SPROUT_REMOTE_VBUCKET_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Sprout: Memcached remote vBucket communication error cleared",
          "Sprout communication to Memcached for a remote piece of data has been restored."
        },
        {MAJOR,
          "Sprout: Memcached remote vBucket communication error",
          "Sprout is unable to contact any Memcacheds for a remote piece of data. Some subscriber data will "
          "be unavailable. If this alarm does not clear, ensure that all remote Memcacheds are operational "
          "and that network connectivity exists between them and Sprout."
        }
      }
    },

    {HOMESTEAD_PROCESS_FAIL, SOFTWARE_ERROR,
      {
        {CLEARED,
          "Homestead: Process failure cleared",
          "The Homestead process has been restored to normal operation."
        },
        {CRITICAL,
          "Homestead: Process failure",
          "Monit has detected that the Homestead process has failed. A restart will automatically be "
          "attempted. If this alarm does not clear, the Homestead process may have been stopped or an "
          "unrecoverable failure may have occurred."
        }
      }
    },

    {HOMESTEAD_CASSANDRA_COMM_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Homestead: Cassandra communication error cleared",
          "Homestead communication to the local Cassandra has been restored."
        },
        {CRITICAL,
          "Homestead: Cassandra communication error",
          "Homestead is unable to contact the local Cassandra. It will periodically attempt to reconnect. "
          "If this alarm does not clear, ensure that the local Cassandra is configured correctly then "
          "restart it."
        }
      }
    },

    {HOMESTEAD_HSS_COMM_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Homestead: HSS communication error cleared",
          "Homestead communication to at least one HSS has been restored."
        },
        {CRITICAL,
          "Homestead: HSS communication error",
          "Homestead is unable to contact any HSSs. It will periodically attempt to reconnect. If this alarm "
          "does not clear, ensure that at least one HSS is operational and that network connectivity exists "
          "between it and Homestead."
        }
      }
    },

    {RALF_PROCESS_FAIL, SOFTWARE_ERROR,
      {
        {CLEARED,
          "Ralf: Process failure cleared",
          "The Ralf process has been restored to normal operation."
        },
        {CRITICAL,
          "Ralf: Process failure",
          "Monit has detected that the Ralf process has failed. A restart will automatically be attempted. "
          "If this alarm does not clear, the Ralf process may have been stopped or an unrecoverable failure "
          "may have occurred."
        }
      }
    },

    {RALF_MEMCACHED_COMM_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Ralf: Memcached communication error cleared",
          "Ralf communication to at least one Memcached has been restored."
        },
        {CRITICAL,
          "Ralf: Memcached communication error",
          "Ralf is unable to contact any Memcacheds. It will periodically attempt to reconnect. If this "
          "alarm does not clear, ensure that at least one Memcached is operational and that network "
          "connectivity exists between it and Ralf."
        }
      }
    },

    {RALF_CHRONOS_COMM_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Ralf: Chronos communication error cleared",
          "Ralf communication to at least one Chronos has been restored."
        },
        {CRITICAL,
          "Ralf: Chronos communication error",
          "Ralf is unable to contact any Chronos's. It will periodically attempt to reconnect. If this "
          "alarm does not clear, ensure that at least one Chronos is operational and that network "
          "connectivity exists between it and Ralf."
        }
      }
    },

    {RALF_CDF_COMM_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Ralf: CDF communication error cleared",
          "Ralf communication to at least one CDF has been restored."
        },
        {CRITICAL,
          "Ralf: CDF communication error",
          "Ralf is unable to contact any CDFs. It will periodically attempt to reconnect. If this alarm "
          "does not clear, ensure that at least one HSS is operational and that network connectivity "
          "exists between it and Ralf."
        }
      }
    },

    {RALF_VBUCKET_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Ralf: Memcached vBucket communication error cleared",
          "Ralf communication to Memcached for a piece of data has been restored."
        },
        {MAJOR,
          "Ralf: Memcached vBucket communication error",
          "Ralf is unable to contact any Memcacheds for a piece of data. Some subscriber data will be "
          "unavailable. If this alarm does not clear, ensure that all Memcacheds are operational and "
          "that network connectivity exists between them and Ralf."
        }
      }
    },

    {CHRONOS_PROCESS_FAIL, SOFTWARE_ERROR,
      {
        {CLEARED,
          "Chronos: Process failure cleared",
          "The Chronos process has been restored to normal operation."
        },
        {MAJOR,
          "Chronos: Process failure",
          "Monit has detected that the Chronos process has failed. A restart will automatically be "
          "attempted. If this alarm does not clear, the Chronos process may have been stopped or an "
          "unrecoverable failure may have occurred."
        }
      }
    },

    {CHRONOS_TIMER_POP_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Chronos: Timer pop error cleared",
          "Chronos local timer delivery restored to normal operation."
        },
        {MAJOR,
          "Chronos: Timer pop error",
          "Chronos was unable to pop a timer on the last replica due to a local delivery error. If this "
          "alarm does not clear, the local Chronos client cannot be contacted and may have failed."
        }
      }
    },

    {MEMCACHED_PROCESS_FAIL, SOFTWARE_ERROR,
      {
        {CLEARED,
          "Memcached: Process failure cleared",
          "The Memcached process has been restored to normal operation."
        },
        {CRITICAL,
          "Memcached: Process failure",
          "Monit has detected that the Memcached process has failed. A restart will automatically be "
          "attempted. If this alarm does not clear, the Memcached process may have been stopped or an "
          "unrecoverable failure may have occurred."
        }
      }
    },

    {CASSANDRA_PROCESS_FAIL, SOFTWARE_ERROR,
      {
        {CLEARED,
          "Cassandra: Process failure cleared",
          "The Cassandra process has been restored to normal operation."
        },
        {CRITICAL,
          "Cassandra: Process failure",
          "Monit has detected that the Cassandra process has failed. A restart will automatically be "
          "attempted. If this alarm does not clear, the Cassandra process may have been stopped or an "
          "unrecoverable failure may have occurred."
        }
      }
    },

    {CASSANDRA_RING_NODE_FAIL, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Cassandra: Ring node error/failure cleared",
          "All Cassandra ring nodes have been restored to normal operation."
        },
        {MAJOR,
          "Cassandra: Ring node redundancy error",
          "Cassandra is unable to contact one of its ring nodes. It will periodically attempt to "
          "reconnect. If this alarm does not clear, ensure that all Cassandra instances are operational and "
          "verify network connectivity to reporting nodes."
        },
        {CRITICAL,
          "Cassandra: Ring node redundancy failure",
          "Cassandra is unable to contact more than one of its ring nodes. It will periodically attempt to "
          "reconnect. If this alarm does not clear, ensure that all Cassandra instances are operational and "
          "verify network connectivity to reporting nodes."
        }
      }
    },

    {MONIT_PROCESS_FAIL, SOFTWARE_ERROR,
      {
        {CLEARED,
          "Monit: Process failure cleared",
          "The Monit process has been restored to normal operation."
        },
        {WARNING,
          "Monit: Process failure",
          "Upstart has detected that the Monit process has failed. A restart will automatically be "
          "attempted. If this alarm does not clear, an unrecoverable failure may have occurred."
        }
      }
    },

    {MEMENTO_HTTP_SERVER_PROCESS_FAIL, SOFTWARE_ERROR,
      {
        {CLEARED,
          "Memento: HTTP Server process failure cleared",
          "The Memento HTTP Server process has been restored to normal operation."
        },
        {CRITICAL,
          "Memento: HTTP Server process failure",
          "While this condition persists, all attempts to retrieve call lists from this Memento server will "
          "fail, but no call list records will be lost. Monit will automatically attempt to restart the HTTP "
          "server process."
        }
      }
    },

    {MEMENTO_PROXY_SERVER_PROCESS_FAIL, SOFTWARE_ERROR,
      {
        {CLEARED,
          "Memento: Web Proxy Server process failure cleared",
          "The Memento Web Proxy Server process has been restored to normal operation."
        },
        {CRITICAL,
          "Memento: Web Proxy Server process failure",
          "While this condition persists, all attempts to retrieve call lists from this Memento server will "
          "fail, but no call list records will be lost. Monit will automatically attempt to restart the Web Proxy Server "
          "process. "
        }
      }
    },

    {MEMENTO_MEMCACHED_COMM_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Memento: Memcached communication error cleared",
          "Memento communication to at least one Memcached has been restored."
        },
        {CRITICAL,
          "Memento: Memcached communication error",
          "Memento is unable to contact any Memcacheds. It will periodically attempt to reconnect. If this "
          "alarm does not clear, ensure that at least one Memcached is operational and that network "
          "connectivity exists between it and Memento."
        }
      }
    },

    {MEMENTO_MEMCACHED_VBUCKET_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Memento: Memcached vBucket communication error cleared",
          "Memento communication to Memcached for a piece of data has been restored."
        },
        {MAJOR,
          "Memento: Memcached vBucket communication error",
          "Memento is unable to contact any Memcacheds for a piece of data. Some subscriber data will be "
          "unavailable. If this alarm does not clear, ensure that all Memcacheds are operational and "
          "that network connectivity exists between them and Memento."
        }
      }
    },

    {MEMENTO_HOMESTEAD_COMM_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Memento: Homestead communication error cleared",
          "Memento communication to at least one Homestead has been restored."
        },
        {CRITICAL,
          "Memento: Homestead communication error",
          "Memento is unable to contact any Homesteads. It will periodically attempt to reconnect. If this "
          "alarm does not clear, ensure that at least one Homestead is operational and that network "
          "connectivity exists between it and Memento."
        }
      }
    },

    {MEMENTO_CASSANDRA_COMM_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Memento: HTTP Server Cassandra communication error cleared",
          "Memento HTTP Server communication to the local Cassandra has been restored."
        },
        {CRITICAL,
          "Memento: HTTP Server Cassandra communication error",
          "While this condition persists, requests to this server to retrieve call lists will fail, but no "
          "call list records will be lost.  The HTTP Server will periodically attempt to reconnect. "
        }
      }
    },

    {MEMENTO_AS_CASSANDRA_COMM_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Memento: Application Server Cassandra communication error cleared",
          "Memento Application Server communication to the local Cassandra has been restored."
        },
        {CRITICAL,
          "Memento: Application Server Cassandra communication error",
          "While this condition persists, new call list records will not be written and will be lost.  The "
          "Application Server will periodically attempt to reconnect. "
        }
      }
    },

    {MEMENTO_HTTP_SERVER_COMM_ERROR, UNDERLYING_RESOURCE_UNAVAILABLE,
      {
        {CLEARED,
          "Memento: Web Proxy HTTP Server error cleared",
          "Memento Web Proxy communication to the local HTTP Server has been restored."
        },
        {CRITICAL,
          "Memento: Web Proxy HTTP Server communication error",
          "While this condition persists, requests to this server to retrieve call lists will fail, but "
          "no call list records will be lost.  The Web Proxy will attempt to reconnect. "
        }
      }
    },

  };
}

