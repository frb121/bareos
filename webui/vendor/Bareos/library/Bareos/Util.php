<?php

/**
 *
 * bareos-webui - Bareos Web-Frontend
 *
 * @link      https://github.com/bareos/bareos for the canonical source repository
 * @copyright Copyright (c) 2024-2024 Bareos GmbH & Co. KG
 * @license   GNU Affero General Public License (http://www.gnu.org/licenses/)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

namespace Bareos;

class Util
{
    public static function log($var)
    {
        error_log(var_export($var, true));
    }

    public static function getNearestVersionInfo($bareos_supported_versions, $version)
    {
        foreach ($bareos_supported_versions as $version_info) {
            if (version_compare($version, $version_info["version"], ">=")) {
                $version_info["requested_version"] = $version;
                return $version_info;
            }
        }
        return null;
    }

}
