<?php
/**
 * This file handles the Data Release for LORIS
 *
 * PHP Version 7
 *
 * @category LORIS
 * @package  Main
 * @author   Justin Kat <justinkat@gmail.com>
 * @license  http://www.gnu.org/licenses/gpl-3.0.txt GPLv3
 * @link     https://www.github.com/aces/Loris
 */
namespace LORIS\data_release;

/**
 * Data Release Class
 *
 * This class is for the Data Release
 *
 * @category LORIS
 * @package  Main
 * @author   Justin Kat <justinkat@gmail.com>
 * @license  http://www.gnu.org/licenses/gpl-3.0.txt GPLv3
 * @link     https://www.github.com/aces/Loris
 */

class Data_Release extends \NDB_Menu_Filter
{
    public $AjaxModule   = true;
    public $skipTemplate = true;

    /**
     * Check user permissions
     *
     * @param \User $user The user whose access is being checked
     *
     * @return bool
     */
    function _hasAccess(\User $user) : bool
    {
        // check user permissions
        return $user->hasAnyPermission(
            [
                'data_release_view',
                'data_release_upload',
                'data_release_edit_file_access'
            ]
        );
    }

    /**
     * Setup all the class variables needed for the data release menu page
     *
     * @return void
     */
    function _setupVariables()
    {
        $user =& \User::singleton();
        $DB   = $this->loris->getDatabaseConnection();

        // set the class variables
        $this->columns = [
            'file_name AS fileName',
            'IF(version is null or version ="","Unversioned", version) AS version',
            'upload_date AS uploadDate',
            'dr.id as dataReleaseID',
        ];
        $this->query   = " FROM data_release dr";

        if (!$user->hasPermission("superuser")) {
            $this->query .= " JOIN data_release_permissions drp 
            ON (dr.id=drp.data_release_id) 
            JOIN users u ON (u.ID=drp.userid) 
            WHERE u.UserID=".$DB->quote($user->getUsername());
        }

        $this->group_by = '';
        $this->order_by = 'uploadDate';
    }

    /**
     * Create the form for the data release menu page
     *
     * @return void
     **/
    function setup()
    {
        parent::setup();

        $db = $this->loris->getDatabaseConnection();

        $this->fieldOptions = [
            'users'     => $this->getUsersList($db),
            'versions'  => $this->getVersionsList($db),
            'filenames' => $this->getFilesList($db),
        ];
    }


    /**
     * Greps the list of users available in the users database table.
     *
     * @param \Database $DB database handle
     *
     * @return array Array of user ID and user name
     */
    function getUsersList(\Database $DB)
    {
        return $DB->pselectColWithIndexKey(
            "SELECT ID, UserID FROM users",
            [],
            "ID"
        );
    }

    /**
     * Greps the list of files available in the data_release database table.
     *
     * @param \Database $DB database handle
     *
     * @return array Array of file ID and file name
     */
    function getFilesList(\Database $DB)
    {
        $result = $DB->pselectWithIndexKey(
            "SELECT id, file_name, version FROM data_release",
            [],
            "id"
        );

        foreach ($result as $id => $row) {
            $version     = $row['version'] == null || $row['version'] == '' ?
               'Unversioned' : $row['version'];
            $result[$id] = $row['file_name'].' - '.$version;
        }

        return $result;
    }

    /**
     * Greps the list of versions available in the data_release database table.
     *
     * @param \Database $DB database handle
     *
     * @return array $versionList Array of version names indexed by version
     *                            name.
     */
    function getVersionsList(\Database $DB)
    {
        $versions = $DB->pselectCol(
            "SELECT DISTINCT version FROM data_release",
            [],
            "version"
        );

        $versionsList = [];
        foreach ($versions as $version) {
            $version = $version == null || $version == '' ?
                'Unversioned' : $version;
            $versionsList[$version] = $version;
        }

        return $versionsList;
    }

    /**
     * Queries arrays of file IDs indexed by their versions.
     *
     * @param \Database $db database handle
     *
     * @return array $versionFiles Array of file IDs indexed by version name.
     */
    function getVersionFiles(\Database $db)
    {
        $result = $db->pselect(
            "SELECT version, id FROM data_release",
            []
        );

        $versionFiles = [];
        foreach ($result as $row) {
            $version = $row['version'] == null || $row['version'] == '' ?
                'Unversioned' : $row['version'];
            $versionFiles[$version][] = $row['id'];
        }

        return $versionFiles;
    }

    /**
     * Queries arrays of file IDs indexed by their users.
     *
     * @param \Database $db database handle
     *
     * @return array $userFiles Array of file IDs indexed by version name.
     */
    function getUserFiles(\Database $db)
    {
        $result = $db->pselect(
            "SELECT u.ID as userId,
                    u.UserID as userName,
                    drp.data_release_id fileId
             FROM users u
             LEFT JOIN data_release_permissions drp ON (u.ID=drp.userid)
             LEFT JOIN data_release dr ON (drp.data_release_id=dr.id)",
            []
        );

        $userFiles = [];
        foreach ($result as $row) {
            $userFiles[$row['userId']]['name'] = $row['userName'];
            if (empty($userFiles[$row['userId']]['files'])) {
                $userFiles[$row['userId']]['files'] = [];
            }
            if (!empty($row['fileId'])) {
                $userFiles[$row['userId']]['files'][] = $row['fileId'];
            }
        }

        return $userFiles;
    }

    /**
     * Creates a nested array indexed by userId which holds all the versions
     * for which the user has permission across all files.
     *
     * @param \Database $db database handle
     *
     * @return array $userVersionFiles Array of versions for which a user has
     *                                 full permission.
     */
    function getUserVersionFiles(\Database $db)
    {
        $versionFiles = $this->getVersionFiles($db);
        $userFiles    = $this->getUserFiles($db);
        foreach ($userFiles as $userId => $user) {
            foreach ($versionFiles as $version => $files) {
                if (empty($userFiles[$userId]['versions'])) {
                    $userFiles[$userId]['versions'] = [];
                }
                if (array_intersect($files, $user['files']) == $files) {
                    $userFiles[$userId]['versions'][] = (string)$version;
                }
            }
        }

        return $userFiles;
    }

    /**
     * Include the column formatter
     *
     * @return array of javascript to be inserted
     */
    function getJSDependencies()
    {
        $factory = \NDB_Factory::singleton();
        $baseurl = $factory->settings()->getBaseURL();
        return array_merge(
            parent::getJSDependencies(),
            [
                $baseurl . "/data_release/js/dataReleaseIndex.js",
            ]
        );
    }
}
