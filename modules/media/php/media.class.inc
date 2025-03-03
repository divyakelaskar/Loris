<?php declare(strict_types=1);
namespace LORIS\media;

/**
 * Main class for media module corresponding to /media/ URL
 * Child or Clinical section of the LorisMenu.
 *
 * Displays a list of media files and control panel to search them and upload
 * new ones.
 *
 * PHP Version 7
 *
 * @category Module
 * @package  Media
 * @author   Alex Ilea <ailea.mcin@gmail.com>
 * @license  http://www.gnu.org/licenses/gpl-3.0.txt GPLv3
 * @link     https://www.github.com/aces/CCNA/
 */
class Media extends \DataFrameworkMenu
{
    /**
     * Check user permissions
     *
     * @param \User $user The user whose access is being checked
     *
     * @return bool
     */
    function _hasAccess(\User $user) : bool
    {
        return $user->hasAnyPermission(
            [
                'media_read',
                'media_write',
            ]
        );
    }

    /**
     * Tells the base class that this page's provisioner can support the
     * HasAnyPermissionOrUserSiteMatch filter.
     *
     * @return ?array of site permissions or null
     */
    public function allSitePermissionNames() : ?array
    {
        return ['access_all_profiles'];
    }

    /**
     * Tells the base class that this page's provisioner can support
     * the UserProjectMatch filter.
     *
     * @return bool false
     */
    public function useProjectFilter() : bool
    {
        return false;
    }

    /**
     * {@inheritDoc}
     *
     * @return array
     */
    protected function getFieldOptions() : array
    {
        $user = \User::singleton();
        $db   = $this->loris->getDatabaseConnection();

        $siteList  = [];
        $visitList = \Utility::getVisitList();

        // allow to view all sites data through filter
        if ($user->hasPermission('access_all_profiles')) {
            $siteList = \Utility::getSiteList(false);
        } else {
            // allow only to view own site data
            $siteIDs = $user->getCenterIDs();
            foreach ($siteIDs as $val) {
                $site = \Site::singleton($val);
                if ($site->isStudySite()) {
                    $siteList["$val"] = $site->getCenterName();
                }
            }
        }

        $instrumentList   = [];
        $instrumentsQuery = $db->pselect(
            "SELECT Test_name, Full_name FROM test_names ORDER BY Test_name",
            []
        );
        foreach ($instrumentsQuery as $instrument) {
            try {
                $instrumentName = \NDB_BVL_Instrument::factory(
                    $this->loris,
                    $instrument["Test_name"],
                )->getFullname();
            } catch (\Exception $e) {
                $instrumentName = $instrument['Full_name'];
            }

            $instrumentList[$instrumentName] = $instrumentName;
        }

        $fileTypeList  = [];
        $fileTypeQuery = $db->pselect("SELECT file_type FROM media", []);
        foreach ($fileTypeQuery as $filetype) {
            $fileTypeList[$filetype['file_type']] = $filetype['file_type'];
        }

        //Language selector
        $languages    = \Utility::getLanguageList();
        $languageList = [];
        foreach ($languages as $language) {
            $languageList[$language] = $language;
        }

        $hiddenOptions = [];
        if (\User::singleton()->hasPermission('superuser')) {
            $hiddenOptions = [
                'visible' => 'Visible only',
                'hidden'  => 'Hidden only',
            ];
        }

        return [
            'fileTypes'     => $fileTypeList,
            'visits'        => $visitList,
            'sites'         => $siteList,
            'projects'      => \Utility::getProjectList(),
            'instruments'   => $instrumentList,
            'languages'     => $languageList,
            'hidden'        => $hiddenOptions,
            'maxUploadSize' => \Utility::getMaxUploadSize(),
        ];
    }

    /**
     * {@inheritDoc}
     *
     * @return \Loris\Data\Provisioner
     */
    public function getBaseDataProvisioner(): \LORIS\Data\Provisioner
    {
        return new MediaFileProvisioner($this->loris);
    }

    /**
     * Gets the data source for this menu filter.
     *
     * @return \LORIS\Data\Provisioner
     */
    public function getDataProvisionerWithFilters() : \LORIS\Data\Provisioner
    {
        $provisioner = parent::getDataProvisionerWithFilters();
        $user        = \NDB_Factory::singleton()->user();
        if ($user->hasPermission('superuser')) {
            // We don't filter out hidden files.
            return $provisioner;
        }
        return $provisioner->filter(new HideFileFilter());
    }

    /**
     * Include additional CSS files:
     *  1. media.css
     *
     * @return array of javascript to be inserted
     */
    function getCSSDependencies() : array
    {
        $factory = \NDB_Factory::singleton();
        $baseURL = $factory->settings()->getBaseURL();
        $deps    = parent::getCSSDependencies();
        return array_merge(
            $deps,
            [$baseURL . "/media/css/media.css"]
        );
    }

    /**
     * Include additional JS files
     *
     * @return array of javascript to be inserted
     */
    function getJSDependencies() : array
    {
        $factory = \NDB_Factory::singleton();
        $baseURL = $factory->settings()->getBaseURL();
        $deps    = parent::getJSDependencies();
        return array_merge(
            $deps,
            [
                $baseURL . "/media/js/mediaIndex.js",
            ]
        );
    }
}

