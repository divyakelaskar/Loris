<?php
/**
 * Publications module class
 *
 * PHP version 7
 *
 * @category Behavioural
 * @package  Main
 * @author   David Blader  <dblader.mcin@gmail.com>
 * @license  http://www.gnu.org/licenses/gpl-3.0.txt GPLv3
 * @link     https://github.com/mohadesz/Loris-Trunk
 */
namespace LORIS\publication;

/**
 * Module for viewing research project proposals and their respective publications
 *
 * @category Behavioural
 * @package  Main
 * @author   David Blader  <dblader.mcin@gmail.com>
 * @license  http://www.gnu.org/licenses/gpl-3.0.txt GPLv3
 * @link     https://github.com/aces/Loris
 */
class Publication extends \NDB_Menu_Filter_Form
{
    public $AjaxModule   = true;
    public $skipTemplate = true;
    /**
     * Returns true if user has access to this page.
     *
     * You do not need to overload hasAccess() if there are no access restrictions.
     *
     * @param \User $user The user whose access is being checked
     *
     * @return bool
     */
    function _hasAccess(\User $user) : bool
    {
        $factory = \NDB_Factory::singleton();
        $db      = $factory->database();
        $user    = $factory->user();

        $nProjects = $db->pselectOne(
            'SELECT COUNT(*) FROM publication_users_edit_perm_rel '.
            'WHERE UserID=:uid',
            ['uid' => $user->getId()]
        );
        return ($user->hasPermission('publication_view')
            || $user->hasPermission('publication_propose')
            || $user->hasPermission('publication_approve')
            || $nProjects > 0
        );
    }

    /**
     * Sets up the Filter Variables
     *
     * @return void
     */
    function _setupVariables() : void
    {
        $factory = \NDB_Factory::singleton();
        $user    = $factory->user();
        $query   = " FROM publication p ".
            "LEFT JOIN publication_status ps ".
            "ON ps.PublicationStatusID=p.PublicationStatusID ".
            "LEFT JOIN publication_parameter_type_rel pptr ".
            "ON p.PublicationID=pptr.PublicationID ".
            "LEFT JOIN parameter_type pt ".
            "ON pt.ParameterTypeID=pptr.ParameterTypeID " .
            "LEFT JOIN publication_test_names_rel ptnr ".
            "ON p.PublicationID=ptnr.PublicationID ".
            "LEFT JOIN test_names tn ".
            "ON ptnr.TestNameID=tn.ID " .
            "LEFT JOIN publication_collaborator_rel pcr ".
            "ON pcr.PublicationID=p.PublicationID ".
            "LEFT JOIN publication_collaborator pc ".
            "ON pcr.PublicationCollaboratorID=pc.PublicationCollaboratorID ".
            // make separate join for getting LeadInvestigator from collaborator
            // table (pcli)
            "LEFT JOIN publication_collaborator pcli ".
            "ON pcli.PublicationCollaboratorID=p.LeadInvestigatorID ".
            "LEFT JOIN publication_keyword_rel pkr ".
            "ON p.PublicationID=pkr.PublicationID ".
            "LEFT JOIN publication_keyword pk ".
            "ON pkr.PublicationKeywordID=pk.PublicationKeywordID ".
            "LEFT JOIN publication_users_edit_perm_rel puepr ".
            "ON puepr.PublicationID=p.PublicationID ".
            "LEFT JOIN users u ".
            "ON u.ID=p.UserID ".
            "LEFT JOIN Project pr ".
            "ON p.project=pr.ProjectID ";
        $query .= " WHERE 1=1 ";
        // allow user access to module if they have edit
        // access to any project proposal
        // but only those that they do have access to
        if (!($user->hasPermission('publication_view')
            || $user->hasPermission('publication_propose')
            || $user->hasPermission('publication_approve'))
        ) {
            $query .= " AND puepr.UserID={$user->getId()} ";
        }

        $query .= " GROUP BY p.PublicationID ";

        $this->columns = [
            'p.Title',
            'pcli.Name as LeadInvestigator',
            'p.DateProposed',
            'ps.Label', // Status (Approved, Pending, Rejected, etc.)
            'pr.Name as project',
            'p.journal',
            'p.link',
            'p.publishingStatus',
            'p.datePublication',
            'u.Real_name',
            'p.Description',
            'GROUP_CONCAT(DISTINCT pc.Name)', // Collaborators
                          // vv Variables of Interest vv
                          // need to use IFNULL since GROUP_CONCAT seems to fail
                          // if first column has no entries
            'GROUP_CONCAT(
                            DISTINCT IFNULL(pt.Name, ""), IFNULL(tn.Test_name,"")
                          )',
            'GROUP_CONCAT(DISTINCT pk.Label)', // Keywords
            'p.PublicationID',
        ];

        $this->query   = $query;
        $this->headers = [
            'Title',
            'Lead Investigator',
            'Date Proposed',
            'Approval Status',
            'Project',
            'Journal',
            'Link',
            'Publishing Status',
            'Date published',
            'Project Proposal Creator',
            'Description',
            'Collaborators',
            'Variables Of Interest',
            'Keywords',
            'Publication ID',
        ];

        $this->validFilters = [
            'p.Title',
            'p.ApprovalStatus',
            'pc.Name',
            'pk.Label',
            'pcli.Name', // Lead Investigator's name
            'pt.Name',
        ];

        $this->formToFilter = [
            'titleOrDescription'  => 'p.Title',
            'approvalStatus'      => 'ps.Label',
            'collaborators'       => 'pc.Name',
            'keywords'            => 'pk.Label',
            'variablesOfInterest' => 'pt.Name',
            'leadInvestigator'    => 'pcli.Name',
        ];
    }

    /**
     * Sets up the smarty menu filter items for publication
     *
     * @return void
     */
    function setup() : void
    {
        parent::setup();

        $db        = $this->loris->getDatabaseConnection();
        $rawStatus = $db->pselectCol(
            'SELECT Label FROM publication_status',
            []
        );

        $statusOptions = [];
        foreach ($rawStatus as $rs) {
            $statusOptions[$rs] = $rs;
        }

        $rawProject = $db->pselect(
            'SELECT ProjectID, Name FROM Project',
            []
        );

        $projectOptions = [];
        foreach ($rawProject as $data) {
            $projectOptions[$data['Name']] = $data['Name'];
        }

        $this->addBasicText(
            'title',
            'Title'
        );
        $this->addSelect(
            'approvalStatus',
            'Approval Status',
            $statusOptions
        );

        // filters by name
        $this->addBasicText(
            'leadInvestigator',
            'Lead Investigator'
        );

        //also filters by name
        $this->addBasicText(
            'collaborators',
            'Collaborators'
        );

        $this->addBasicText(
            'keywords',
            'Keywords'
        );

        $this->addBasicText(
            'variablesOfInterest',
            'Variables of Interest'
        );

        $this->addBasicText(
            'journal',
            'Journal'
        );

        $this->addSelect(
            'project',
            'Project',
            $projectOptions
        );

        $this->addBasicText(
            'link',
            'Link'
        );

        $this->addSelect(
            'publishingStatus',
            'Publishing Status',
            [
                'Published'   => 'Published',
                'In Progress' => 'In Progress'
            ]
        );
    }


    /**
     * Include the column formatter required to display the feedback link colours
     * in the candidate_list menu
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
            [$baseURL . '/publication/js/publicationIndex.js']
        );
    }

    /**
     * Include additional CSS files:
     *  imaging_uploader.css
     *
     * @return array of css to be inserted
     */
    function getCSSDependencies() : array
    {
        $factory = \NDB_Factory::singleton();
        $baseURL = $factory->settings()->getBaseURL();
        $deps    = parent::getCSSDependencies();
        return array_merge(
            $deps,
            [$baseURL . '/publication/css/publication.css']
        );
    }

    /**
     * Converts the results of this menu filter to a JSON format to be retrieved
     * with ?format=json
     *
     * @return string json encoded string of the headers and data from this table
     */
    function toJSON() : string
    {
        $result = $this->toArray();
        // run values through htmlspecialchars decode to unescape
        // special chars
        array_walk_recursive(
            $result,
            function (&$r) {
                $r = htmlspecialchars_decode($r);
            }
        );
        $result['form'] = $this->form->form;

        return json_encode($result);
    }
}

